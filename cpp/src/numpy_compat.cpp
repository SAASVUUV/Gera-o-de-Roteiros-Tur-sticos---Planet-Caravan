#include "numpy_compat.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <limits>
#include <system_error>

namespace numpy_compat {

// ---------------------------------------------------------------------------
// Gerador aleatório (numpy/random/src/mt19937 e distributions.c, modo legado)
// ---------------------------------------------------------------------------

double RandomState::random_sample() {
    // mt19937_next_double
    const int32_t a = static_cast<int32_t>(next_uint32() >> 5);
    const int32_t b = static_cast<int32_t>(next_uint32() >> 6);
    return (a * 67108864.0 + b) / 9007199254740992.0;
}

int64_t RandomState::randint(int64_t low, int64_t high) {
    // _rand_int64 -> random_bounded_uint64_fill com use_masked = true
    if (high <= low) {
        throw std::invalid_argument("low >= high");
    }
    const uint64_t rng = static_cast<uint64_t>(high - 1 - low);

    // intervalo de um único valor: o numpy não consome nenhum número aleatório
    if (rng == 0) {
        return low;
    }

    if (rng <= 0xFFFFFFFFULL) {
        if (rng == 0xFFFFFFFFULL) {
            return low + static_cast<int64_t>(next_uint32());
        }
        // gen_mask: menor máscara 2^k - 1 que cobre rng
        uint64_t mask = rng;
        mask |= mask >> 1;
        mask |= mask >> 2;
        mask |= mask >> 4;
        mask |= mask >> 8;
        mask |= mask >> 16;
        const uint32_t mask32 = static_cast<uint32_t>(mask);
        // buffered_bounded_masked_uint32: rejeita até cair no intervalo
        uint32_t value;
        while ((value = (next_uint32() & mask32)) > rng) {
        }
        return low + static_cast<int64_t>(value);
    }

    // Os índices do GRASP nunca passam de 2^32; não implementado.
    throw std::invalid_argument("randint: range above 2^32 not supported");
}

size_t RandomState::choice_index(size_t pop_size, const std::vector<double>& p) {
    // Validações de RandomState.choice
    if (pop_size == 0) {
        throw std::invalid_argument("'a' cannot be empty unless no samples are taken");
    }
    if (p.size() != pop_size) {
        throw std::invalid_argument("'a' and 'p' must have same size");
    }
    for (double value : p) {
        if (value < 0) {
            throw std::invalid_argument("probabilities are not non-negative");
        }
    }
    // kahan_sum
    double kahan = p[0], c = 0.0;
    for (size_t i = 1; i < p.size(); ++i) {
        const double y = p[i] - c;
        const double t = kahan + y;
        c = (t - kahan) - y;
        kahan = t;
    }
    const double atol = std::sqrt(std::numeric_limits<double>::epsilon());
    if (std::abs(kahan - 1.0) > atol) {
        throw std::invalid_argument("probabilities do not sum to 1");
    }

    // cdf = p.cumsum(); cdf /= cdf[-1]
    std::vector<double> cdf(p.size());
    double accumulated = 0.0;
    for (size_t i = 0; i < p.size(); ++i) {
        accumulated += p[i];
        cdf[i] = accumulated;
    }
    const double last = cdf.back();
    for (double& value : cdf) {
        value /= last;
    }

    // idx = cdf.searchsorted(random_sample(), side='right')
    const double uniform = random_sample();
    return static_cast<size_t>(std::upper_bound(cdf.begin(), cdf.end(), uniform) - cdf.begin());
}

// ---------------------------------------------------------------------------
// np.sum / np.mean
// ---------------------------------------------------------------------------

// DOUBLE_pairwise_sum de numpy/_core/src/umath/loops_utils.h
static double pairwise_sum(const double* a, size_t n) {
    if (n < 8) {
        double res = 0.;
        for (size_t i = 0; i < n; ++i) {
            res += a[i];
        }
        return res;
    }
    if (n <= 128) {
        double r[8];
        for (size_t j = 0; j < 8; ++j) {
            r[j] = a[j];
        }
        size_t i;
        for (i = 8; i < n - (n % 8); i += 8) {
            for (size_t j = 0; j < 8; ++j) {
                r[j] += a[i + j];
            }
        }
        double res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; ++i) {
            res += a[i];
        }
        return res;
    }
    size_t n2 = n / 2;
    n2 -= n2 % 8;
    return pairwise_sum(a, n2) + pairwise_sum(a + n2, n - n2);
}

double sum(const std::vector<double>& values) {
    // a redução começa do elemento neutro (0.0) e soma todos os elementos
    return 0.0 + pairwise_sum(values.data(), values.size());
}

double mean(const std::vector<double>& values) {
    return sum(values) / static_cast<double>(values.size());
}

// ---------------------------------------------------------------------------
// np.unique / np.setdiff1d
// ---------------------------------------------------------------------------

std::vector<int> unique(const std::vector<int>& a) {
    std::vector<int> result(a);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

std::vector<int> setdiff1d(const std::vector<int>& a, const std::vector<int>& b) {
    const std::vector<int> unique_a = unique(a);
    const std::vector<int> unique_b = unique(b);
    std::vector<int> result;
    result.reserve(unique_a.size());
    std::set_difference(unique_a.begin(), unique_a.end(), unique_b.begin(), unique_b.end(),
                        std::back_inserter(result));
    return result;
}

// ---------------------------------------------------------------------------
// str(np.array(...)) — numpy/_core/arrayprint.py, arrays 1-D, linewidth 75
// ---------------------------------------------------------------------------

static std::string rstrip(const std::string& text) {
    const size_t end = text.find_last_not_of(' ');
    return end == std::string::npos ? std::string() : text.substr(0, end + 1);
}

// _formatArray para 1 dimensão: separador " " e quebra de linha em 75 colunas
static std::string format_1d(const std::vector<std::string>& words) {
    if (words.empty()) {
        return "[]";
    }
    const size_t line_width = 75;
    const size_t elem_width = line_width - 1; // - len("]")
    const std::string hanging_indent = " ";

    std::string s, line = hanging_indent;
    for (size_t i = 0; i < words.size(); ++i) {
        // _extendLine
        bool needs_wrap = line.size() + words[i].size() > elem_width;
        if (line.size() <= hanging_indent.size()) {
            needs_wrap = false;
        }
        if (needs_wrap) {
            s += rstrip(line) + "\n";
            line = hanging_indent;
        }
        line += words[i];
        if (i + 1 < words.size()) {
            line += " ";
        }
    }
    s += line;
    return "[" + s.substr(hanging_indent.size()) + "]";
}

std::string array_str(const std::vector<int>& values) {
    // IntegerFormat: todos alinhados à direita na largura do maior
    std::vector<std::string> words;
    size_t width = 0;
    for (int value : values) {
        words.push_back(std::to_string(value));
        width = std::max(width, words.back().size());
    }
    for (std::string& word : words) {
        word.insert(0, width - word.size(), ' ');
    }
    return format_1d(words);
}

// Dígitos de x em notação posicional, como dragon4_positional com
// unique=True, precision=8 e trim='.' (ex.: 41.5 -> "41.5", 42 -> "42.").
static std::string positional(double value) {
    char buffer[400];
    auto [end, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed);
    if (ec != std::errc()) {
        throw std::runtime_error("float format failed");
    }
    std::string text(buffer, end);
    const size_t dot = text.find('.');
    if (dot != std::string::npos && text.size() - dot - 1 > 8) {
        // mais de 8 casas decimais: arredonda para 8
        std::snprintf(buffer, sizeof(buffer), "%.8f", value);
        text = buffer;
    }
    if (text.find('.') == std::string::npos) {
        text += ".";
    }
    while (text.back() == '0') {
        text.pop_back();
    }
    return text;
}

// Como dragon4_scientific com unique=True, precision=8 e trim='.':
// devolve (mantissa, expoente), ex.: 5e-05 -> ("5.", -5), 1.5e-07 -> ("1.5", -7).
static std::pair<std::string, int> scientific(double value) {
    char buffer[64];
    auto [end, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
    if (ec != std::errc()) {
        throw std::runtime_error("float format failed");
    }
    std::string text(buffer, end);
    size_t e = text.find('e');
    const size_t dot = text.find('.');
    if (dot != std::string::npos && e - dot - 1 > 8) {
        // mais de 8 casas decimais: arredonda para 8
        std::snprintf(buffer, sizeof(buffer), "%.8e", value);
        text = buffer;
        e = text.find('e');
    }
    std::string mantissa = text.substr(0, e);
    if (mantissa.find('.') == std::string::npos) {
        mantissa += ".";
    }
    while (mantissa.back() == '0') {
        mantissa.pop_back();
    }
    return {mantissa, std::stoi(text.substr(e + 1))};
}

std::string array_str(const std::vector<double>& values) {
    // FloatingFormat com floatmode='maxprec', precision=8, trim='.'

    // Notação científica se algum valor não nulo for >= 1e8 ou < 1e-4,
    // ou se o maior for mais de 1000 vezes o menor.
    bool exponential = false;
    double max_value = 0, min_value = std::numeric_limits<double>::infinity();
    for (double value : values) {
        if (std::isfinite(value) && value != 0) {
            max_value = std::max(max_value, std::abs(value));
            min_value = std::min(min_value, std::abs(value));
        }
    }
    if (max_value > 0) {
        exponential = max_value >= 1e8 || min_value < 0.0001 || max_value / min_value > 1000.;
    }

    std::vector<std::string> int_parts, frac_parts, exponents;
    size_t pad_left = 0, pad_right = 0, exp_size = 0;
    for (double value : values) {
        if (!std::isfinite(value)) {
            int_parts.push_back(std::isnan(value) ? "nan" : (value > 0 ? "inf" : "-inf"));
            frac_parts.push_back("");
            exponents.push_back("");
            continue;
        }
        std::string text;
        if (exponential) {
            auto [mantissa, exponent] = scientific(value);
            text = mantissa;
            exponents.push_back(std::to_string(std::abs(exponent)));
            exponents.back().insert(0, exponent < 0 ? "-" : "+");
            exp_size = std::max(exp_size, exponents.back().size() - 1);
        } else {
            text = positional(value);
        }
        const size_t dot = text.find('.');
        int_parts.push_back(text.substr(0, dot));
        frac_parts.push_back(text.substr(dot + 1));
        pad_left = std::max(pad_left, int_parts.back().size());
        pad_right = std::max(pad_right, frac_parts.back().size());
    }
    exp_size = std::max<size_t>(exp_size, 2); // o expoente tem pelo menos 2 dígitos

    std::vector<std::string> words;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!std::isfinite(values[i])) {
            // nan/inf ocupam a largura total do campo
            const size_t width = exponential ? pad_left + pad_right + 1 + exp_size + 2
                                             : pad_left + pad_right + 1;
            words.push_back(std::string(width > int_parts[i].size() ? width - int_parts[i].size() : 0, ' ') +
                            int_parts[i]);
            continue;
        }
        std::string word(pad_left - int_parts[i].size(), ' ');
        word += int_parts[i] + "." + frac_parts[i];
        if (exponential) {
            // na notação científica todos ficam com o mesmo número de casas
            word += std::string(pad_right - frac_parts[i].size(), '0');
            const std::string& exponent = exponents[i];
            const std::string digits = exponent.substr(1);
            word += "e" + exponent.substr(0, 1) + std::string(exp_size - digits.size(), '0') + digits;
        } else {
            word += std::string(pad_right - frac_parts[i].size(), ' ');
        }
        words.push_back(word);
    }
    return format_1d(words);
}

std::string py_float_str(double value) {
    // repr(float) do Python: dígitos mais curtos; notação científica se o
    // expoente decimal for < -4 ou >= 16.
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return value > 0 ? "inf" : "-inf";
    }
    char buffer[64];
    auto [end, ec] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
    if (ec != std::errc()) {
        throw std::runtime_error("float format failed");
    }
    const std::string scientific(buffer, end); // ex.: "-3.827e-02"
    const size_t e = scientific.find('e');
    const int exponent = std::stoi(scientific.substr(e + 1));
    std::string sign, digits;
    for (size_t i = 0; i < e; ++i) {
        if (scientific[i] == '-') {
            sign = "-";
        } else if (scientific[i] != '.') {
            digits += scientific[i];
        }
    }

    if (exponent < -4 || exponent >= 16) {
        std::string mantissa = digits.substr(0, 1);
        if (digits.size() > 1) {
            mantissa += "." + digits.substr(1);
        }
        char exp_text[16];
        std::snprintf(exp_text, sizeof(exp_text), "e%c%02d", exponent < 0 ? '-' : '+', std::abs(exponent));
        return sign + mantissa + exp_text;
    }

    const int decimal_point = exponent + 1; // posição do ponto em relação aos dígitos
    std::string text;
    if (decimal_point <= 0) {
        text = "0." + std::string(static_cast<size_t>(-decimal_point), '0') + digits;
    } else if (static_cast<size_t>(decimal_point) >= digits.size()) {
        text = digits + std::string(static_cast<size_t>(decimal_point) - digits.size(), '0') + ".0";
    } else {
        text = digits.substr(0, static_cast<size_t>(decimal_point)) + "." +
               digits.substr(static_cast<size_t>(decimal_point));
    }
    return sign + text;
}

} // namespace numpy_compat

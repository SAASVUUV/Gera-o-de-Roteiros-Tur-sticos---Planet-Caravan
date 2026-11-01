// Verifica a viabilidade de cada solução de um arquivo de resultados
// (formato do src/opt_call.py e do cpp/build/grasp).
//
// Rode a partir da raiz do repositório:
//
//     cpp/build/check_results "Results Analysis/My Results/results.txt"
//         [--output ARQUIVO.tsv] [--summary ARQUIVO.txt] [--instance Input/RJ_instance]
//         [--amount-days N] [--daily-places N] [--category N]
//
// O arquivo de resultados só traz budget e total_distance de cada linha. Os
// outros parâmetros do usuário (amount_days, amount_daily_places, category)
// vêm de Parameters/user.json, que precisa ter um único valor para cada um,
// ou das opções acima.
//
// Linhas de comentário ("# GRASP max_iterations=10") viram a coluna "secao",
// para separar os grupos no resumo.
//
// Saída:
//   - um TSV com uma linha por solução e as colunas da verificação
//     (padrão: <arquivo>_checked.tsv, ao lado do original);
//   - um resumo por grupo, mostrado no terminal e salvo em arquivo
//     (padrão: <arquivo>_summary.txt, ao lado do original).

#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "../instance.hpp"
#include "../model/feasibility.hpp"
#include "../operations/aux.hpp"

namespace {

struct Options {
    std::string input;
    std::string output;
    std::string summary;
    std::string instance = "Input/RJ_instance";
    std::optional<int> amount_days, daily_places, category;
};

struct Record {
    std::string section;
    std::vector<std::string> fields;
    size_t line = 0;
};

std::string trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string strip_brackets(const std::string& text) {
    std::string result = trim(text);
    if (!result.empty() && result.front() == '[') result.erase(0, 1);
    if (!result.empty() && result.back() == ']') result.pop_back();
    return trim(result);
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string part;
    std::istringstream stream(text);
    while (std::getline(stream, part, separator)) {
        parts.push_back(part);
    }
    return parts;
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(argument + " precisa de um valor");
            }
            return argv[++i];
        };
        if (argument == "--output") {
            options.output = next();
        } else if (argument == "--summary") {
            options.summary = next();
        } else if (argument == "--instance") {
            options.instance = next();
        } else if (argument == "--amount-days") {
            options.amount_days = std::stoi(next());
        } else if (argument == "--daily-places") {
            options.daily_places = std::stoi(next());
        } else if (argument == "--category") {
            options.category = std::stoi(next());
        } else if (argument == "-h" || argument == "--help") {
            std::cout << "uso: check_results ARQUIVO [--output ARQUIVO.tsv] [--summary ARQUIVO.txt] [--instance DIR]\n"
                         "                    [--amount-days N] [--daily-places N] [--category N]\n";
            std::exit(0);
        } else if (!argument.empty() && argument[0] == '-') {
            throw std::invalid_argument("argumento desconhecido: " + argument);
        } else if (options.input.empty()) {
            options.input = argument;
        } else {
            throw std::invalid_argument("só um arquivo de resultados por vez");
        }
    }
    if (options.input.empty()) {
        throw std::invalid_argument("informe o arquivo de resultados (use --help)");
    }
    const std::filesystem::path path(options.input);
    if (options.output.empty()) {
        options.output = (path.parent_path() / (path.stem().string() + "_checked.tsv")).string();
    }
    if (options.summary.empty()) {
        options.summary = (path.parent_path() / (path.stem().string() + "_summary.txt")).string();
    }
    return options;
}

// Parâmetro fixo: opção da linha de comando ou valor único de user.json
int fixed_parameter(const std::optional<int>& option, const Json& user_json, const std::string& key,
                    const std::string& flag) {
    if (option.has_value()) {
        return *option;
    }
    const Json& values = user_json.at(key);
    if (values.size() != 1) {
        throw std::invalid_argument("Parameters/user.json tem " + std::to_string(values.size()) + " valores para " +
                                    key + "; informe qual usar com " + flag);
    }
    return values[0].get<int>();
}

// Lê o arquivo juntando as linhas de continuação: o numpy quebra rotas longas
// em várias linhas, e as continuações começam com espaço.
std::vector<Record> read_records(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("não foi possível abrir " + path);
    }
    std::vector<Record> records;
    std::string section, line, text;
    size_t line_number = 0, record_line = 0;

    auto flush = [&]() {
        if (!trim(text).empty()) {
            records.push_back({section, split(text, '\t'), record_line});
        }
        text.clear();
    };

    while (std::getline(file, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (trim(line).empty()) {
            continue;
        }
        if (line[0] == '#') {
            flush();
            section = trim(line.substr(1));
        } else if (line[0] == ' ' && !text.empty()) {
            text += line; // continuação da rota
        } else {
            flush();
            text = line;
            record_line = line_number;
        }
    }
    flush();
    return records;
}

std::string join(const std::vector<std::string>& values, const std::string& separator) {
    std::string result;
    for (size_t i = 0; i < values.size(); ++i) {
        result += (i ? separator : "") + values[i];
    }
    return result;
}

template <class T> std::string join_numbers(const std::vector<T>& values, int precision) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision);
    for (size_t i = 0; i < values.size(); ++i) {
        stream << (i ? "," : "") << values[i];
    }
    return stream.str();
}

std::string current_time() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", std::localtime(&now));
    return buffer;
}

// Tipo da violação, sem dia nem valor: "distancia_dia2(+3314.2)" -> "distancia"
std::string violation_type(const std::string& violation) {
    return violation.substr(0, violation.find_first_of("_("));
}

struct GroupSummary {
    std::string section, algorithm, budget, total_distance;
    int count = 0, feasible = 0;
    double fitness_sum = 0, feasible_fitness_sum = 0;
    double best_feasible = -1;
    std::map<std::string, int> violations; // tipo -> soluções com essa violação
};

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);

        const PlacesParameters places_parameters = read_instance(options.instance);

        const Json user_json = read_parameters("user.json");
        UsersParameters users;
        users.amount_days = fixed_parameter(options.amount_days, user_json, "amount_days", "--amount-days");
        users.amount_daily_places =
            fixed_parameter(options.daily_places, user_json, "amount_daily_places", "--daily-places");
        users.category = fixed_parameter(options.category, user_json, "category", "--category");

        const std::vector<Record> records = read_records(options.input);

        std::ofstream output(options.output);
        if (!output) {
            throw std::runtime_error("não foi possível escrever em " + options.output);
        }
        output << "secao\talgorithm\tbudget\ttotal_distance\ttime\tfitness\tfitness_recalculado\tviavel\tcusto"
                  "\tdistancia_max\tdistancias_por_dia\tlocais_por_dia\tmax_categoria_seguida\tviolacoes\troute\n";

        std::vector<GroupSummary> groups;
        std::map<std::string, size_t> group_index;
        int fitness_mismatches = 0;

        for (const Record& record : records) {
            if (record.fields.size() < 6) {
                throw std::runtime_error(options.input + ":" + std::to_string(record.line) +
                                         ": linha com menos de 6 colunas");
            }
            const std::string& algorithm = record.fields[0];
            users.budget = std::stod(record.fields[1]);
            users.total_distance = std::stod(record.fields[2]);
            const double reported_fitness = std::stod(strip_brackets(record.fields[4]));

            std::vector<int> route;
            std::istringstream route_stream(strip_brackets(record.fields[5]));
            for (int place; route_stream >> place;) {
                route.push_back(place);
            }

            const FeasibilityReport report = check_feasibility(route, places_parameters, users);

            if (std::abs(report.fitness - reported_fitness) > 1e-9) {
                ++fitness_mismatches;
            }

            const double max_distance = report.daily_distance.empty()
                                            ? 0
                                            : *std::max_element(report.daily_distance.begin(),
                                                                report.daily_distance.end());

            output << record.section << '\t' << algorithm << '\t' << record.fields[1] << '\t' << record.fields[2]
                   << '\t' << record.fields[3] << '\t' << reported_fitness << '\t' << report.fitness << '\t'
                   << (report.feasible ? 1 : 0) << '\t' << report.cost << '\t' << std::fixed
                   << std::setprecision(1) << max_distance << std::defaultfloat << std::setprecision(6) << '\t'
                   << join_numbers(report.daily_distance, 1) << '\t' << join_numbers(report.daily_places, 0)
                   << '\t' << report.max_consecutive_category << '\t'
                   << (report.violations.empty() ? "-" : join(report.violations, ";")) << '\t' << "["
                   << join_numbers(route, 0) << "]" << '\n';

            // resumo por grupo (seção, algoritmo, budget, distância)
            const std::string key = record.section + '\x1f' + algorithm + '\x1f' + record.fields[1] + '\x1f' +
                                    record.fields[2];
            if (!group_index.count(key)) {
                group_index[key] = groups.size();
                GroupSummary group;
                group.section = record.section;
                group.algorithm = algorithm;
                group.budget = record.fields[1];
                group.total_distance = record.fields[2];
                groups.push_back(group);
            }
            GroupSummary& group = groups[group_index[key]];
            group.count += 1;
            group.fitness_sum += reported_fitness;
            if (report.feasible) {
                group.feasible += 1;
                group.feasible_fitness_sum += reported_fitness;
                group.best_feasible = std::max(group.best_feasible, reported_fitness);
            }
            std::map<std::string, bool> types;
            for (const std::string& violation : report.violations) {
                types[violation_type(violation)] = true;
            }
            for (const auto& [type, present] : types) {
                group.violations[type] += present;
            }
        }

        // --- resumo: vai para o terminal e para o arquivo de resumo --------
        std::ostringstream summary;
        summary << "Resumo de viabilidade de: " << options.input << "\n"
                << "Gerado em: " << current_time() << " por cpp/build/check_results\n"
                << "Dados: " << options.instance << "\n";
        summary << "Restrições: amount_days=" << users.amount_days
                  << ", amount_daily_places=" << users.amount_daily_places << ", category=" << users.category
                  << " (seguidos)\n\n";

        // largura da coluna "secao": o maior nome de grupo
        int label_width = 6;
        for (const GroupSummary& group : groups) {
            const std::string& label = group.section.empty() ? group.algorithm : group.section;
            label_width = std::max(label_width, static_cast<int>(label.size()) + 2);
        }

        summary << std::left << std::setw(label_width) << "secao" << std::setw(8) << "budget" << std::setw(9) << "dist"
                  << std::right << std::setw(6) << "n" << std::setw(9) << "viaveis" << std::setw(12)
                  << "fit.todas" << std::setw(12) << "fit.viaveis" << std::setw(12) << "melhor.viav"
                  << "  violacoes\n";

        int total = 0, total_feasible = 0;
        for (const GroupSummary& group : groups) {
            total += group.count;
            total_feasible += group.feasible;
            std::string label = group.section.empty() ? group.algorithm : group.section;
            std::ostringstream violations;
            for (const auto& [type, count] : group.violations) {
                violations << type << "=" << count << " ";
            }
            summary << std::left << std::setw(label_width) << label << std::setw(8) << group.budget << std::setw(9)
                      << group.total_distance << std::right << std::setw(6) << group.count << std::setw(8)
                      << std::fixed << std::setprecision(0) << 100.0 * group.feasible / group.count << "%"
                      << std::setprecision(2) << std::setw(12) << group.fitness_sum / group.count;
            if (group.feasible > 0) {
                summary << std::setw(12) << group.feasible_fitness_sum / group.feasible << std::setw(12)
                          << group.best_feasible;
            } else {
                summary << std::setw(12) << "-" << std::setw(12) << "-";
            }
            summary << "  " << violations.str() << "\n";
        }

        summary << "\nTotal: " << total_feasible << " de " << total << " soluções viáveis ("
                  << std::setprecision(1) << 100.0 * total_feasible / std::max(total, 1) << "%)\n";
        if (fitness_mismatches > 0) {
            summary << "AVISO: em " << fitness_mismatches
                      << " linhas o fitness recalculado difere do registrado. O arquivo provavelmente foi "
                         "gerado com outros dados (outra matriz de distâncias ou outro RJ.csv).\n";
        }
        summary << "\nColunas: n = soluções no grupo; viaveis = % que respeitam todas as restrições;\n"
                   "fit.todas / fit.viaveis = fitness médio de todas / só das viáveis; melhor.viav = maior\n"
                   "fitness entre as viáveis; violacoes = quantas soluções violam cada restrição.\n"
                   "Regras: orçamento = hotel x dias + locais; distância de cada dia inclui a volta ao\n"
                   "hotel; categoria = locais seguidos da mesma categoria; nenhum local se repete na viagem.\n";

        std::cout << summary.str();

        std::ofstream summary_file(options.summary);
        if (!summary_file) {
            throw std::runtime_error("não foi possível escrever em " + options.summary);
        }
        summary_file << summary.str();

        std::cout << "Resumo salvo em: " << options.summary << "\n";
        std::cout << "Detalhes por solução em: " << options.output << "\n";

    } catch (const std::exception& error) {
        std::cerr << "erro: " << error.what() << "\n";
        return 1;
    }
    return 0;
}

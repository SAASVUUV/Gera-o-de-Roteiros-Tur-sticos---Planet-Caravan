// Reproduz em C++ as partes do numpy usadas pelo GRASP, com o mesmo resultado
// bit a bit:
//   - o gerador aleatório legado (np.random.seed / randint / choice / random_sample);
//   - np.sum / np.mean (soma "pairwise" do numpy);
//   - np.setdiff1d / np.unique;
//   - str(np.array(...)), usado para escrever os resultados.
//
// Com isso, a mesma semente gera exatamente as mesmas rotas no Python e no C++.

#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace numpy_compat {

// Equivalente a np.random.RandomState (o estado global de np.random.*).
// O np.random.seed(s) legado inicializa o Mersenne Twister do mesmo jeito que
// std::mt19937(s), então a sequência de inteiros de 32 bits é a mesma.
class RandomState {
  public:
    explicit RandomState(uint32_t seed = 1) : mt_(seed) {}

    void seed(uint32_t seed) { mt_.seed(seed); }

    uint32_t next_uint32() { return static_cast<uint32_t>(mt_()); }

    // np.random.random_sample(): double em [0, 1) com 53 bits.
    double random_sample();

    // np.random.randint(low, high): inteiro em [low, high).
    int64_t randint(int64_t low, int64_t high);

    // np.random.choice(a, 1)[0]: sorteio uniforme.
    template <class T> T choice(const std::vector<T>& a) {
        if (a.empty()) {
            throw std::invalid_argument("'a' cannot be empty unless no samples are taken");
        }
        return a[static_cast<size_t>(randint(0, static_cast<int64_t>(a.size())))];
    }

    // np.random.choice(a, 1, p=p)[0]: sorteio com probabilidades.
    template <class T> T choice(const std::vector<T>& a, const std::vector<double>& p) {
        return a[choice_index(a.size(), p)];
    }

  private:
    size_t choice_index(size_t pop_size, const std::vector<double>& p);

    std::mt19937 mt_;
};

// np.sum de um array float64 (soma pairwise do numpy, na mesma ordem).
double sum(const std::vector<double>& values);

// np.mean de um array float64.
double mean(const std::vector<double>& values);

// np.setdiff1d(a, b): valores únicos de a que não estão em b, ordenados.
std::vector<int> setdiff1d(const std::vector<int>& a, const std::vector<int>& b);

// np.unique(a): valores únicos, ordenados.
std::vector<int> unique(const std::vector<int>& a);

// Acesso com índice no estilo Python/numpy: a[-1] é o último elemento.
template <class T> const T& py_at(const std::vector<T>& a, long index) {
    const long size = static_cast<long>(a.size());
    const long real = index < 0 ? index + size : index;
    if (real < 0 || real >= size) {
        throw std::out_of_range("index " + std::to_string(index) + " is out of bounds");
    }
    return a[static_cast<size_t>(real)];
}

// str(np.array(values)) para um array de inteiros, ex.: "[ 555 1071 1354]".
std::string array_str(const std::vector<int>& values);

// str(np.array(values)) para um array float64, ex.: "[41.5]" ou "[42.]".
std::string array_str(const std::vector<double>& values);

// str(x) de um float do Python (repr mais curto), usado na coluna de tempo.
std::string py_float_str(double value);

} // namespace numpy_compat

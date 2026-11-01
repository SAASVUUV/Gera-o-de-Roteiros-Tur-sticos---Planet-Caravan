// Equivalente a src/main.py + src/opt_call.py, só para o Greedy (tradução literal).
//
// Rode a partir da raiz do repositório:
//
//     cpp/build/greedy [--instance Input/RJ_instance]
//                      [--output "Results Analysis/My Results/results_greedy_cpp.txt"]
//
// Lê Parameters/user.json e experiments.json e, como o Python, roda o Greedy
// para cada combinação de parâmetros do usuário x amount_executions,
// acrescentando uma linha por execução no arquivo de saída, no mesmo formato
// do Python. O Greedy é determinístico: todas as execuções de uma combinação
// dão a mesma rota.

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "../heuristics/simple_greedy.hpp"
#include "../instance.hpp"
#include "../numpy_compat.hpp"
#include "../operations/aux.hpp"

namespace {

struct Options {
    std::string instance = "Input/RJ_instance";
    std::string output = "Results Analysis/My Results/results_greedy_cpp.txt";
};

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
        if (argument == "--instance") {
            options.instance = next();
        } else if (argument == "--output") {
            options.output = next();
        } else if (argument == "-h" || argument == "--help") {
            std::cout << "uso: greedy [--instance DIR] [--output ARQUIVO]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("argumento desconhecido: " + argument);
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);

        const PlacesParameters places_parameters = read_instance(options.instance);

        const std::vector<Json> users_combinations = explode_parameters(read_parameters("user.json"));
        const int amount_executions = read_parameters("experiments.json").at("amount_executions").get<int>();

        const std::string algorithm = "Greedy";

        write_comment(options.output, algorithm);

        for (const Json& user_parameter : users_combinations) {

            const UsersParameters users = users_parameters_from_json(user_parameter);

            for (int execution = 0; execution < amount_executions; ++execution) {

                const auto start = std::chrono::steady_clock::now();

                const auto [solution, f] = construct_greedy_route(places_parameters, users);

                const auto end = std::chrono::steady_clock::now();

                // no Python, o fitness do Greedy é um float (str: "36.0"), não um array
                write_results(options.output, user_parameter.at("budget"), user_parameter.at("total_distance"),
                              std::chrono::duration<double>(end - start).count(), numpy_compat::py_float_str(f),
                              numpy_compat::array_str(solution), algorithm);
            }
        }

        std::cerr << "Resultados em: " << options.output << "\n";

    } catch (const std::exception& error) {
        std::cerr << "erro: " << error.what() << "\n";
        return 1;
    }
    return 0;
}

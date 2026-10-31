// Equivalente a src/main.py + src/opt_call.py, só para o GRASP.
//
// Rode a partir da raiz do repositório (os caminhos são relativos a ela):
//
//     cpp/build/grasp [--instance Input/RJ_instance]
//                     [--output "Results Analysis/My Results/results_cpp.txt"]
//                     [--seed 1]
//
// Lê Parameters/user.json, algorithm.json e experiments.json e, como o Python,
// roda o GRASP para cada combinação de parâmetros do algoritmo x combinação de
// parâmetros do usuário x amount_executions, acrescentando uma linha por
// execução no arquivo de saída. Um único gerador aleatório é usado na execução
// toda; com a semente 1 (a do main.py) as rotas saem iguais às do Python.

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "heuristics/grasp.hpp"
#include "instance.hpp"
#include "numpy_compat.hpp"
#include "operations/aux.hpp"

namespace {

struct Options {
    std::string instance = "Input/RJ_instance";
    std::string output = "Results Analysis/My Results/results_cpp.txt";
    uint32_t seed = 1;
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
        } else if (argument == "--seed") {
            options.seed = static_cast<uint32_t>(std::stoul(next()));
        } else if (argument == "-h" || argument == "--help") {
            std::cout << "uso: grasp [--instance DIR] [--output ARQUIVO] [--seed N]\n";
            std::exit(0);
        } else {
            throw std::invalid_argument("argumento desconhecido: " + argument);
        }
    }
    return options;
}

void generate_experiments(const PlacesParameters& places_parameters, const Options& options) {

    numpy_compat::RandomState random(options.seed); // np.random.seed(1)

    // we have to explode the parameters
    const Json users_parameters = read_parameters("user.json");
    const Json algorithm_parameters = read_parameters("algorithm.json");
    const Json experiments_parameters = read_parameters("experiments.json");

    const std::vector<Json> grasp_parameters = explode_parameters(algorithm_parameters.at("GRASP"));
    const std::vector<Json> users_combinations = explode_parameters(users_parameters);
    const int amount_executions = experiments_parameters.at("amount_executions").get<int>();

    const std::string algorithm = "GRASP";

    for (const Json& algo_parameters : grasp_parameters) {

        const GraspParameters grasp = grasp_parameters_from_json(algo_parameters);

        // list of parameters
        for (const Json& user_parameter : users_combinations) {

            const UsersParameters users = users_parameters_from_json(user_parameter);

            std::cerr << algorithm << " " << algo_parameters.dump() << " budget=" << user_parameter.at("budget")
                      << " total_distance=" << user_parameter.at("total_distance") << " ... " << std::flush;

            const auto combination_start = std::chrono::steady_clock::now();

            for (int execution = 0; execution < amount_executions; ++execution) {

                const auto start = std::chrono::steady_clock::now();

                const auto [solution, f] = grasp_heuristic(places_parameters, users, grasp, random);

                const auto end = std::chrono::steady_clock::now();

                // str(f) no Python é str de um array numpy de um elemento: "[41.5]"
                write_results(options.output, user_parameter.at("budget"), user_parameter.at("total_distance"),
                              std::chrono::duration<double>(end - start).count(),
                              numpy_compat::array_str(std::vector<double>{f}), numpy_compat::array_str(solution),
                              algorithm);
            }

            const double elapsed =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - combination_start).count();
            std::cerr << amount_executions << " execuções em " << elapsed << " s\n";
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);

        const PlacesParameters places_parameters = read_instance(options.instance);

        generate_experiments(places_parameters, options);

        std::cerr << "Resultados em: " << options.output << "\n";

    } catch (const std::exception& error) {
        std::cerr << "erro: " << error.what() << "\n";
        return 1;
    }
    return 0;
}

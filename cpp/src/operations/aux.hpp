// Tradução de src/operations/aux.py

#pragma once

#include <string>
#include <vector>

#include "../../third_party/nlohmann/json.hpp"
#include "../parameters.hpp"

// ordered_json mantém a ordem das chaves do arquivo, como o dict do Python.
using Json = nlohmann::ordered_json;

void write_results(const std::string& output_file, const Json& budget, const Json& distance,
                   double time, const std::string& fitness, const std::string& route,
                   const std::string& algorithm);

Json read_parameters(const std::string& file_parameters);

// Produto cartesiano entre todas as listas (itertools.product): uma combinação
// por objeto, com a última chave variando mais rápido.
std::vector<Json> explode_parameters(const Json& parameters);

// Conversão de uma combinação explodida para as structs do C++
UsersParameters users_parameters_from_json(const Json& combination);
GraspParameters grasp_parameters_from_json(const Json& combination);

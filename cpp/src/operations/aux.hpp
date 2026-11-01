// Tradução de src/operations/aux.py

#pragma once

#include <string>
#include <vector>

#include "../../third_party/nlohmann/json.hpp"
#include "../parameters.hpp"

// ordered_json mantém a ordem das chaves do arquivo, como o dict do Python.
using Json = nlohmann::ordered_json;

// `extra`, se não for vazio, vira uma 7ª coluna (ex.: a origem da solução no
// GRASP_v3). O formato do Python tem 6 colunas; o check_results aceita as duas.
void write_results(const std::string& output_file, const Json& budget, const Json& distance,
                   double time, const std::string& fitness, const std::string& route,
                   const std::string& algorithm, const std::string& extra = "");

// Linha de comentário ("# texto") para separar grupos no arquivo de
// resultados. Não existe no Python; o check_results usa como "secao".
void write_comment(const std::string& output_file, const std::string& text);

Json read_parameters(const std::string& file_parameters);

// Produto cartesiano entre todas as listas (itertools.product): uma combinação
// por objeto, com a última chave variando mais rápido.
std::vector<Json> explode_parameters(const Json& parameters);

// Conversão de uma combinação explodida para as structs do C++
UsersParameters users_parameters_from_json(const Json& combination);
GraspParameters grasp_parameters_from_json(const Json& combination);

// GRASP dos autores (src/heuristics/grasp.py) traduzido para C++.
//
// Versões (todas no git; ver a tabela em cpp/README.md):
//   - tradução literal, idêntica ao Python: commit 70eb95a
//   - GRASP_v1: busca local respeita orçamento e distância (marcado "CORRIGIDO:"),
//     commit c09da9d
//   - GRASP_v2 (esta): a busca local ordena os candidatos pelo efeito no
//     trajeto e testa em ordem até um caber (ver order_candidates)
// Os comportamentos originais que continuam iguais estão marcados com "ORIGINAL:".

#pragma once

#include <map>
#include <utility>
#include <vector>

#include "../instance.hpp"
#include "../numpy_compat.hpp"
#include "../parameters.hpp"

// Nome do algoritmo na coluna "algorithm" dos resultados. É diferente de
// "GRASP" para não misturar com os resultados dos autores.
inline constexpr const char* GRASP_NAME = "GRASP_v2";

// Um item do dict alphas = {0.3: {"results": [...]}, 0.4: ..., 0.7: ...}
struct AlphaResults {
    double alpha;
    std::vector<double> results;
};

bool verify_local_search_changes(RouteParameters& route_parameters,
                                 const UsersParameters& users_parameters,
                                 const PlacesParameters& places_parameters, int old_place,
                                 int new_place, long position, int day);

// Ordem em que a busca local testa os candidatos para substituir `place`,
// que fica entre `before` e `after` na rota do dia (after = hotel se place é o
// último). Grupo 1: Δdistância <= 0, maior ganho de nota primeiro. Grupo 2:
// Δdistância > 0, menor Δdistância/ganho primeiro.
std::vector<int> order_candidates(const std::vector<int>& candidates, int place, int before, int after,
                                  const PlacesParameters& places_parameters);

// Devolve (route_parameters, fitness)
std::pair<RouteParameters, double> local_search(RouteParameters route_parameters,
                                                const UsersParameters& users_parameters,
                                                const PlacesParameters& places_parameters,
                                                numpy_compat::RandomState& random);

std::vector<double> inverse_probability(const std::vector<double>& probabilities);

// Devolve a posição do alpha sorteado em alphas
size_t select_alpha(double fitness, const std::vector<AlphaResults>& alphas,
                    numpy_compat::RandomState& random);

std::vector<int> generate_daily_array_route(const UsersParameters& users_parameters,
                                            const RouteParameters& route_parameters);

// Devolve (rota, fitness), como grasp_heuristic no Python
std::pair<std::vector<int>, double> grasp_heuristic(const PlacesParameters& places_parameters,
                                                    const UsersParameters& users_parameters,
                                                    const GraspParameters& algorithm_parameters,
                                                    numpy_compat::RandomState& random);

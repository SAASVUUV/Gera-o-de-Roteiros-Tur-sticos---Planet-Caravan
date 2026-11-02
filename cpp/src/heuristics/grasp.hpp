// GRASP dos autores (src/heuristics/grasp.py) traduzido para C++.
//
// Versões (todas no git; ver a tabela em cpp/README.md):
//   - tradução literal, idêntica ao Python: commit 70eb95a
//   - GRASP_v1: busca local respeita orçamento e distância (marcado "CORRIGIDO:"),
//     commit c09da9d
//   - GRASP_v2: a busca local ordena os candidatos pelo efeito no trajeto e
//     testa em ordem até um caber (ver order_candidates), commit 32fdc39
//   - GRASP_v3: o roteiro do Greedy é a solução inicial (semente); uma
//     solução do GRASP só o substitui se tiver fitness >= ao dele
//   - GRASP_v4 (esta): α reativo com a fórmula de maximização (ver select_alpha)
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
inline constexpr const char* GRASP_NAME = "GRASP_v4";

// Amplificação δ do α reativo (GRASP_v4): q_i = (A_i / z*)^δ. Quanto maior,
// mais o α de melhor média é favorecido; 10 é o valor de Prais & Ribeiro (2000).
inline constexpr double REACTIVE_AMPLIFICATION = 10.0;

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

// Probabilidade de cada alpha: p_i = q_i / soma(q), com q_i = (A_i / z*)^δ,
// A_i = média do fitness das soluções feitas com o alpha i e z* = fitness.
std::vector<double> alpha_probabilities(double fitness, const std::vector<AlphaResults>& alphas);

// Devolve a posição do alpha sorteado em alphas
size_t select_alpha(double fitness, const std::vector<AlphaResults>& alphas,
                    numpy_compat::RandomState& random);

std::vector<int> generate_daily_array_route(const UsersParameters& users_parameters,
                                            const RouteParameters& route_parameters);

// De onde veio a solução final: do Greedy (a semente não foi alcançada) ou de
// uma iteração do GRASP (que alcançou ou superou o Greedy).
enum class SolutionOrigin { greedy, grasp };

inline const char* origin_name(SolutionOrigin origin) {
    return origin == SolutionOrigin::greedy ? "greedy" : "grasp";
}

struct GraspResult {
    std::vector<int> route; // hotel no início de cada dia
    double fitness;
    SolutionOrigin origin;
};

// Como grasp_heuristic no Python (rota, fitness), mais a origem da solução.
GraspResult grasp_heuristic(const PlacesParameters& places_parameters, const UsersParameters& users_parameters,
                            const GraspParameters& algorithm_parameters, numpy_compat::RandomState& random);

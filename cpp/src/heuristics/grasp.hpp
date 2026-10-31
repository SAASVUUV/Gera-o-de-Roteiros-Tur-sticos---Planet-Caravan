// Tradução literal de src/heuristics/grasp.py
//
// A lógica é a mesma do Python, inclusive os comportamentos estranhos, que
// estão marcados com "ORIGINAL:" e listados em cpp/README.md.

#pragma once

#include <map>
#include <utility>
#include <vector>

#include "../instance.hpp"
#include "../numpy_compat.hpp"
#include "../parameters.hpp"

// Um item do dict alphas = {0.3: {"results": [...]}, 0.4: ..., 0.7: ...}
struct AlphaResults {
    double alpha;
    std::vector<double> results;
};

bool verify_local_search_changes(RouteParameters& route_parameters,
                                 const UsersParameters& users_parameters,
                                 const PlacesParameters& places_parameters, int old_place,
                                 int new_place, long index, int day);

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

// Tradução de src/heuristics/random_greedy_procedure.py
// (fase construtiva do GRASP)

#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "../instance.hpp"
#include "../numpy_compat.hpp"
#include "../parameters.hpp"

// Sorteia o próximo local entre os candidatos da lista restrita (RCL).
// Devolve std::nullopt (None no Python) se não houver candidato.
std::optional<int> build_rcl(const std::vector<int>& route, const std::vector<int>& general_route,
                             const UsersParameters& users_parameters,
                             const PlacesParameters& places_parameters,
                             const RouteParameters& route_parameters, int current_day, double alpha,
                             numpy_compat::RandomState& random);

// Sorteia um hotel cujas diárias cabem no orçamento. Devolve (hotel, custo).
std::pair<int, double> select_hotel(const PlacesParameters& places_parameters,
                                    const UsersParameters& users_parameters,
                                    numpy_compat::RandomState& random);

RouteParameters random_greedy(const PlacesParameters& places_parameters,
                              const UsersParameters& users_parameters, double alpha,
                              numpy_compat::RandomState& random);

// Tradução de src/operations/constructive_operations.py

#pragma once

#include <vector>

#include "../instance.hpp"
#include "../parameters.hpp"

// Verifica se new_place pode entrar no fim de route (o dia current_day).
// Se puder, atualiza route_parameters (distância, categoria e custo) e
// devolve true. A rota em si é atualizada por quem chama.
bool verify_constructive_heuristic_constraints(const std::vector<int>& route, int new_place,
                                               const PlacesParameters& places_parameters,
                                               const UsersParameters& users_parameters,
                                               RouteParameters& route_parameters, int current_day);

// Rota "plana": rota inteira do dia 0 e, dos outros dias, a rota sem o hotel.
std::vector<int> generate_route_array(const UsersParameters& users_parameters,
                                      const RouteParameters& route_parameters);

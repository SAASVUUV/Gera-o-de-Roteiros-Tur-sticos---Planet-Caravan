// Tradução literal de src/heuristics/simple_greedy.py
//
// A lógica é a mesma do Python, inclusive os comportamentos estranhos, que
// estão marcados com "ORIGINAL:". O Greedy é determinístico (não sorteia nada).

#pragma once

#include <stdexcept>
#include <utility>
#include <vector>

#include "../instance.hpp"
#include "../parameters.hpp"

// Lançada quando nenhum hotel serve para o Greedy (no Python: IndexError).
struct GreedyHotelNotFound : std::out_of_range {
    using std::out_of_range::out_of_range;
};

// O primeiro hotel (menor ID) com nota 5 cujas diárias cabem no orçamento.
// Devolve (hotel, custo das diárias). Se nenhum hotel servir, lança
// GreedyHotelNotFound, como o IndexError do Python (hotels[0] de um array vazio).
std::pair<int, double> greedy_hotel_selection(const PlacesParameters& places_parameters,
                                              const UsersParameters& users_parameters);

// Devolve (rota, fitness), como construct_greedy_route no Python. A rota tem o
// hotel no início de cada dia, como a do GRASP.
std::pair<std::vector<int>, double> construct_greedy_route(const PlacesParameters& places_parameters,
                                                           const UsersParameters& users_parameters);

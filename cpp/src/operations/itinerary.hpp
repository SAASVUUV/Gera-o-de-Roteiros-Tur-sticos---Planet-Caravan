// Mostra uma rota (IDs, com o hotel no início de cada dia) como roteiro legível,
// com os nomes dos locais (Input/RJ_instance/names.tsv). Se os nomes não foram
// exportados, mostra "#ID" no lugar.

#pragma once

#include <string>
#include <vector>

#include "../instance.hpp"
#include "../parameters.hpp"

// Nome do local, ou "#ID" se names.tsv não foi exportado.
std::string place_name(int place, const PlacesParameters& places_parameters);

// Uma linha: "Casa 48 | Dia 1: A > B > C > D | Dia 2: E > F > G > H"
std::string compact_itinerary(const std::vector<int>& route, const PlacesParameters& places_parameters);

// Várias linhas: hotel, cada dia com os locais na ordem (tipo, nota, preço e
// distância de cada trecho, incluindo a volta ao hotel), custo e fitness.
std::string detailed_itinerary(const std::vector<int>& route, const PlacesParameters& places_parameters,
                               const UsersParameters& users_parameters);

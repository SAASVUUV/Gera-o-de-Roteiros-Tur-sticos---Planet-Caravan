// Equivalente ao dict places_parameters de src/opt_call.py.
//
// Os dados são lidos da pasta gerada por scripts/export_instance.py. Como no
// Python, cada local é um inteiro (ID) que indexa todos os vetores e a matriz
// de distâncias, e os hotéis têm os menores IDs.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class Category { hotel, restaurant, attraction };

struct PlacesParameters {
    std::vector<int> hotels;            // places_parameters["hotels"]
    std::vector<double> price;          // places_parameters["price"]  (Max Price)
    std::vector<double> rating;         // places_parameters["rating"] (Mean Sentiment)
    std::vector<Category> category;     // places_parameters["category"]
    std::vector<int> places_available;  // places_parameters["places_available"]
    // places_available na ordem em que o Greedy os percorre (nota decrescente,
    // empates na ordem do argsort do numpy; ver scripts/export_instance.py)
    std::vector<int> greedy_order;
    // Só para exibir roteiros (names.tsv); ficam vazios se o arquivo não existir.
    std::vector<std::string> names; // nome do local
    std::vector<std::string> types; // tipo original do CSV, ex.: "Praias"
    std::vector<double> distance_data;  // places_parameters["distance_matrix"], linha a linha
    size_t size = 0;                    // quantidade de locais

    // distance_matrix[from][to]
    double distance(int from, int to) const {
        return distance_data[static_cast<size_t>(from) * size + static_cast<size_t>(to)];
    }
};

PlacesParameters read_instance(const std::string& directory);

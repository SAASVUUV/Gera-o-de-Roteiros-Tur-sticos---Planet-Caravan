// Verificador de viabilidade independente das heurísticas.
//
// Recalcula tudo a partir da rota final, sem confiar nos valores que a
// heurística guardou durante a busca (no GRASP dos autores esses valores
// ficam errados depois da busca local).
//
// Restrições (definições do artigo, com as escolhas documentadas em cpp/README.md):
//   - Orçamento: preço do hotel x amount_days + preço de cada local <= budget
//   - Distância diária: hotel -> locais do dia, na ordem -> hotel <= total_distance
//   - Locais por dia: quantidade de locais do dia (sem o hotel) <= amount_daily_places
//   - Categoria: no máximo `category` locais seguidos da mesma categoria no dia
//   - Repetição: nenhum local se repete na viagem inteira
//   - Estrutura: a rota começa num hotel, tem amount_days dias, e nenhum
//     outro hotel aparece como local

#pragma once

#include <string>
#include <vector>

#include "../instance.hpp"
#include "../parameters.hpp"

// Folga para erros de arredondamento ao comparar somas de floats com os
// limites (metros e reais). Uma soma feita em outra ordem pode diferir do
// valor da heurística no último bit; 1e-6 é muito menor que qualquer
// diferença real.
inline constexpr double FEASIBILITY_TOLERANCE = 1e-6;

struct FeasibilityReport {
    bool feasible = true;

    double cost = 0;                    // hotel x dias + locais
    std::vector<double> daily_distance; // com a volta ao hotel
    std::vector<int> daily_places;      // locais por dia, sem o hotel
    int max_consecutive_category = 0;   // maior sequência de mesma categoria num dia
    double fitness = 0;                 // soma das notas dos locais distintos, hotel incluído

    // Uma entrada por violação, ex.: "distancia_dia2(+3314.2)", "orcamento(+51)"
    std::vector<std::string> violations;
};

// Separa a rota no formato de saída das heurísticas (hotel no início de cada
// dia, ex.: [h a b c h d e f]) em uma lista por dia, cada uma começando no hotel.
std::vector<std::vector<int>> split_days(const std::vector<int>& route);

// Verifica uma rota no formato de saída das heurísticas.
FeasibilityReport check_feasibility(const std::vector<int>& route, const PlacesParameters& places_parameters,
                                    const UsersParameters& users_parameters);

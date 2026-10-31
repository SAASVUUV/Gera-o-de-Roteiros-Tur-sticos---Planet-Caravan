// Tradução de src/model/objective_function.py

#pragma once

#include <vector>

// Soma das notas dos locais distintos da rota, hotel incluído.
// No Python, measure_fitness recebe uma lista de rotas e devolve um array com
// um fitness por rota; o GRASP sempre passa uma rota só ([route]) e usa
// fitness[0], então aqui a função recebe uma rota e devolve um número.
double measure_fitness(const std::vector<int>& route, const std::vector<double>& ratings);

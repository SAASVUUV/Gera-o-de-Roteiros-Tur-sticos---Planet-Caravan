#include "objective_function.hpp"

#include "../numpy_compat.hpp"

double measure_fitness(const std::vector<int>& route, const std::vector<double>& ratings) {

    // np.sum(ratings[np.unique(np.array(route, dtype=int))])
    std::vector<double> selected;
    for (int place : numpy_compat::unique(route)) {
        selected.push_back(ratings[static_cast<size_t>(place)]);
    }

    return numpy_compat::sum(selected);
}

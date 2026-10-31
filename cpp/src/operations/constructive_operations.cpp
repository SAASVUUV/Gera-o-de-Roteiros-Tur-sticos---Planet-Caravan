#include "constructive_operations.hpp"

#include <algorithm>

#include "../numpy_compat.hpp"

using numpy_compat::py_at;

bool verify_constructive_heuristic_constraints(const std::vector<int>& route, int new_place,
                                               const PlacesParameters& places_parameters,
                                               const UsersParameters& users_parameters,
                                               RouteParameters& route_parameters, int current_day) {

    DayParameters& day = route_parameters.days[static_cast<size_t>(current_day)];

    const double total_cost = route_parameters.budget + places_parameters.price[static_cast<size_t>(new_place)];

    if (static_cast<long>(route.size()) + 1 > users_parameters.amount_daily_places) {

        return false;
    }

    if (total_cost > users_parameters.budget) {

        return false;
    }

    const double back_to_hotel = places_parameters.distance(py_at(route, -1), new_place) +
                                 places_parameters.distance(new_place, route[0]);

    if (day.daily_distance + back_to_hotel > users_parameters.total_distance) {

        return false;
    }

    const Category new_place_category = places_parameters.category[static_cast<size_t>(new_place)];

    if (day.category_count(new_place_category) + 1 > users_parameters.category) {

        return false;
    }

    if (std::find(route.begin(), route.end(), new_place) != route.end()) {

        return false;
    }

    // atualizando os parâmetros da rota
    day.daily_distance += places_parameters.distance(py_at(route, -1), new_place);

    day.category_count(new_place_category) += 1;

    route_parameters.budget = total_cost;

    return true;
}

std::vector<int> generate_route_array(const UsersParameters& users_parameters,
                                      const RouteParameters& route_parameters) {

    std::vector<int> route;

    for (int day = 0; day < users_parameters.amount_days; ++day) {

        const std::vector<int>& day_route = route_parameters.days[static_cast<size_t>(day)].route;

        if (day == 0) {

            route.insert(route.end(), day_route.begin(), day_route.end());

        } else {

            // route_parameters[day]["route"][1:]
            route.insert(route.end(), day_route.begin() + (day_route.empty() ? 0 : 1), day_route.end());
        }
    }

    return route;
}

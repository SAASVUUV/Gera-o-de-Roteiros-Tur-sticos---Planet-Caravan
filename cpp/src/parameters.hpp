// Structs que substituem os dicts do Python:
//   users_parameters      -> UsersParameters
//   algorithm_parameters  -> GraspParameters
//   route_parameters      -> RouteParameters

#pragma once

#include <stdexcept>
#include <vector>

#include "instance.hpp"

// Uma combinação de Parameters/user.json. start_time, end_time e travel_end
// não afetam as rotas no Python (o código que os usa está morto), então não
// aparecem aqui.
struct UsersParameters {
    double budget = 0;
    double total_distance = 0;
    int category = 0;
    int amount_daily_places = 0;
    int amount_days = 0;
};

// Uma combinação de Parameters/algorithm.json["GRASP"].
struct GraspParameters {
    int max_iterations = 0;
};

// route_parameters[day]
struct DayParameters {
    double daily_distance = 0;
    int restaurant = 0;
    int attraction = 0;
    int amount_places = 0; // inicializado e nunca usado, como no Python
    std::vector<int> route;

    // route_parameters[day][category], onde category é "restaurant" ou "attraction"
    int& category_count(Category category) {
        switch (category) {
        case Category::restaurant:
            return restaurant;
        case Category::attraction:
            return attraction;
        default:
            // no Python seria KeyError: route_parameters[day]["hotel"] não existe
            throw std::out_of_range("route_parameters[day] has no key 'hotel'");
        }
    }
};

// route_parameters = {"budget": ..., 0: {...}, 1: {...}, ...}
struct RouteParameters {
    double budget = 0;
    std::vector<DayParameters> days;
};

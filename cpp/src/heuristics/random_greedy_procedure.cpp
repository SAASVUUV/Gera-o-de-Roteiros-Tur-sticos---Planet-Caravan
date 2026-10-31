#include "random_greedy_procedure.hpp"

#include <algorithm>

#include "../operations/constructive_operations.hpp"

using numpy_compat::py_at;

std::optional<int> build_rcl(const std::vector<int>& route, const std::vector<int>& general_route,
                             const UsersParameters& users_parameters,
                             const PlacesParameters& places_parameters,
                             const RouteParameters& route_parameters, int current_day, double alpha,
                             numpy_compat::RandomState& random) {

    const DayParameters& day = route_parameters.days[static_cast<size_t>(current_day)];

    // removendo locais que já foram visitados
    const std::vector<int> visited_removed =
        numpy_compat::setdiff1d(places_parameters.places_available, general_route);

    // removendo com preço alto
    std::vector<int> pois;
    for (int poi : visited_removed) {
        const double visiting_cost = places_parameters.price[static_cast<size_t>(poi)] + route_parameters.budget;
        if (visiting_cost < users_parameters.budget) {
            pois.push_back(poi);
        }
    }

    // distância até o local mais a volta para o hotel
    // ORIGINAL: a "volta" usa distance_matrix[hotel][poi], e não [poi][hotel]
    std::vector<int> candidates;
    for (int poi : pois) {
        const double back_to_hotel = day.daily_distance + places_parameters.distance(py_at(route, -1), poi) +
                                     places_parameters.distance(route[0], poi);
        if (back_to_hotel <= users_parameters.total_distance) {
            candidates.push_back(poi);
        }
    }

    // neste caso não há nenhum local disponível
    if (candidates.empty()) {

        return std::nullopt;
    }

    // distancia do local atual para o proximo, dividido pela nota do local
    std::vector<double> visit_gain;
    visit_gain.reserve(candidates.size());
    for (int candidate : candidates) {
        visit_gain.push_back(places_parameters.distance(py_at(route, -1), candidate) /
                             places_parameters.rating[static_cast<size_t>(candidate)]);
    }

    // beta = cmin + alpha * (cmax - cmin)

    // cmin
    const double min_gain = *std::min_element(visit_gain.begin(), visit_gain.end());

    // cmax
    const double beta = min_gain + alpha * (*std::max_element(visit_gain.begin(), visit_gain.end()) - min_gain);

    // locais que são candidatos
    std::vector<int> rcl;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (visit_gain[i] <= beta) {
            rcl.push_back(candidates[i]);
        }
    }

    const int selected_place = random.choice(rcl);

    return selected_place;
}

std::pair<int, double> select_hotel(const PlacesParameters& places_parameters,
                                    const UsersParameters& users_parameters,
                                    numpy_compat::RandomState& random) {

    // tem que ser hotel, e o valor das diárias não pode ultrapassar o budget
    std::vector<int> hotels;
    for (int hotel : places_parameters.hotels) {
        if (places_parameters.price[static_cast<size_t>(hotel)] * users_parameters.amount_days <
            users_parameters.budget) {
            hotels.push_back(hotel);
        }
    }

    const int selected_hotel = random.choice(hotels);

    return {selected_hotel, places_parameters.price[static_cast<size_t>(selected_hotel)] * users_parameters.amount_days};
}

RouteParameters random_greedy(const PlacesParameters& places_parameters,
                              const UsersParameters& users_parameters, double alpha,
                              numpy_compat::RandomState& random) {

    const int size_places = (users_parameters.amount_days * users_parameters.amount_daily_places) + 1;

    // função escolhe hotel
    const auto [hotel, cost] = select_hotel(places_parameters, users_parameters, random);

    RouteParameters route_parameters;
    route_parameters.budget = cost;

    for (int day = 0; day < users_parameters.amount_days; ++day) {

        DayParameters day_parameters;
        day_parameters.route = {hotel};
        route_parameters.days.push_back(day_parameters);
    }

    std::vector<int> route{hotel}, general_route{hotel};
    long route_size = 0;
    int count_infeasible = 0, current_day = 0;

    // ORIGINAL: a segunda condição é sempre verdadeira; o laço só termina pelo break
    while (route_size + 1 != size_places || current_day + 1 <= users_parameters.amount_days) {

        // atualização automática do alpha
        const std::optional<int> new_place = build_rcl(route, general_route, users_parameters,
                                                       places_parameters, route_parameters,
                                                       current_day, alpha, random);

        if (new_place.has_value()) {

            const bool feasible = verify_constructive_heuristic_constraints(
                route, *new_place, places_parameters, users_parameters, route_parameters, current_day);

            if (feasible) {

                count_infeasible = 0;

                route.push_back(*new_place);

                general_route.push_back(*new_place);

            } else {

                count_infeasible += 1;
            }
        }

        // ORIGINAL: route inclui o hotel, então cada dia tem no máximo
        // amount_daily_places - 1 locais além dele
        if (!new_place.has_value() || static_cast<long>(route.size()) >= users_parameters.amount_daily_places ||
            count_infeasible >= 5) {

            // não é possível adicionar mais nenhum local
            // então adicionamos o hotel
            // e a quantidade dias aumenta
            count_infeasible = 0;

            // removing the hotels
            route_size += static_cast<long>(route.size()) - 1;

            route_parameters.days[static_cast<size_t>(current_day)].route = route;

            if (current_day + 1 >= users_parameters.amount_days) {

                break;
            }

            current_day += 1;

            route = route_parameters.days[static_cast<size_t>(current_day)].route;
        }
    }

    return route_parameters;
}

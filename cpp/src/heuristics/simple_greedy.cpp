#include "simple_greedy.hpp"

#include <algorithm>
#include <stdexcept>

#include "../model/objective_function.hpp"
#include "../operations/constructive_operations.hpp"
#include "grasp.hpp"

std::pair<int, double> greedy_hotel_selection(const PlacesParameters& places_parameters,
                                              const UsersParameters& users_parameters) {
    // o melhor hotel que é mais barato que o o budget

    // tem que ser hotel, e o valor das diárias não pode ultrapassar o budget
    std::vector<int> hotels;
    for (int hotel : places_parameters.hotels) {
        if (places_parameters.rating[static_cast<size_t>(hotel)] == 5 &&
            places_parameters.price[static_cast<size_t>(hotel)] * users_parameters.amount_days <
                users_parameters.budget) {
            hotels.push_back(hotel);
        }
    }

    if (hotels.empty()) {
        // no Python: IndexError em hotels[0]
        throw GreedyHotelNotFound("greedy_hotel_selection: nenhum hotel com nota 5 cabe no orçamento");
    }

    return {hotels[0], places_parameters.price[static_cast<size_t>(hotels[0])] * users_parameters.amount_days};
}

std::pair<std::vector<int>, double> construct_greedy_route(const PlacesParameters& places_parameters,
                                                           const UsersParameters& users_parameters) {
    // Assumption:
    // Dado que estou em current time X
    // quem é o melhor local a visitar que esta mais próximo e respeita as restrições?
    //
    // ORIGINAL: apesar do comentário acima, a distância não entra na escolha:
    // os locais são percorridos só por nota, do maior para o menor.

    const auto [hotel, cost] = greedy_hotel_selection(places_parameters, users_parameters);

    RouteParameters route_parameters;
    route_parameters.budget = cost;

    for (int day = 0; day < users_parameters.amount_days; ++day) {

        DayParameters day_parameters;
        day_parameters.route = {hotel};
        route_parameters.days.push_back(day_parameters);
    }

    // places_available ordenado pela nota, do maior para o menor
    // (np.flip(np.argsort(...)), exportado em greedy_order.txt)
    const std::vector<int>& pois = places_parameters.greedy_order;

    int current_day = 0;
    std::vector<int> route{hotel}, all_visited{hotel};

    while (current_day != users_parameters.amount_days) {

        // remove os locais que já foram visitados

        for (int place : pois) {

            if (std::find(all_visited.begin(), all_visited.end(), place) != all_visited.end()) {

                continue;
            }

            const bool feasible = verify_constructive_heuristic_constraints(
                route, place, places_parameters, users_parameters, route_parameters, current_day);

            if (feasible) {

                route.push_back(place);

                all_visited.push_back(place);
            }

            // ORIGINAL: route inclui o hotel e a verificação já impede mais de
            // amount_daily_places - 1 locais, então esta condição nunca é
            // verdadeira e o laço sempre percorre todos os locais
            if (static_cast<long>(route.size()) - 1 == users_parameters.amount_daily_places) {

                break;
            }
        }

        route_parameters.days[static_cast<size_t>(current_day)].route = route;

        if (current_day + 1 >= users_parameters.amount_days) {

            break;
        }

        current_day += 1;

        route = route_parameters.days[static_cast<size_t>(current_day)].route;
    }

    const std::vector<int> flat_route = generate_route_array(users_parameters, route_parameters);

    // generate_daily_array_route do simple_greedy.py é idêntica à do grasp.py
    return {generate_daily_array_route(users_parameters, route_parameters),
            measure_fitness(flat_route, places_parameters.rating)};
}

#include "feasibility.hpp"

#include <algorithm>
#include <cstdio>
#include <map>

#include "objective_function.hpp"

namespace {

std::string format_excess(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "+%.1f", value);
    return buffer;
}

void add_violation(FeasibilityReport& report, const std::string& violation) {
    report.feasible = false;
    report.violations.push_back(violation);
}

} // namespace

double day_route_distance(const std::vector<int>& day_route, const PlacesParameters& places_parameters) {
    double distance = 0;
    for (size_t i = 0; i + 1 < day_route.size(); ++i) {
        distance += places_parameters.distance(day_route[i], day_route[i + 1]);
    }
    distance += places_parameters.distance(day_route.back(), day_route.front());
    return distance;
}

std::vector<std::vector<int>> split_days(const std::vector<int>& route) {
    std::vector<std::vector<int>> days;
    if (route.empty()) {
        return days;
    }
    const int hotel = route[0];
    for (int place : route) {
        if (place == hotel) {
            days.push_back({hotel});
        } else {
            days.back().push_back(place);
        }
    }
    return days;
}

FeasibilityReport check_feasibility(const std::vector<int>& route, const PlacesParameters& places_parameters,
                                    const UsersParameters& users_parameters) {
    FeasibilityReport report;

    // --- estrutura ---------------------------------------------------------
    if (route.empty()) {
        add_violation(report, "estrutura(rota_vazia)");
        return report;
    }
    for (int place : route) {
        if (place < 0 || static_cast<size_t>(place) >= places_parameters.size) {
            add_violation(report, "estrutura(id_invalido:" + std::to_string(place) + ")");
            return report;
        }
    }

    const int hotel = route[0];
    if (places_parameters.category[static_cast<size_t>(hotel)] != Category::hotel) {
        add_violation(report, "estrutura(nao_comeca_em_hotel:" + std::to_string(hotel) + ")");
    }

    const std::vector<std::vector<int>> days = split_days(route);
    if (static_cast<int>(days.size()) != users_parameters.amount_days) {
        add_violation(report, "estrutura(dias=" + std::to_string(days.size()) +
                                  "!=" + std::to_string(users_parameters.amount_days) + ")");
    }

    report.fitness = measure_fitness(route, places_parameters.rating);

    // --- orçamento -----------------------------------------------------------
    report.cost = places_parameters.price[static_cast<size_t>(hotel)] * users_parameters.amount_days;
    for (const std::vector<int>& day : days) {
        for (size_t i = 1; i < day.size(); ++i) {
            report.cost += places_parameters.price[static_cast<size_t>(day[i])];
        }
    }
    if (report.cost > users_parameters.budget + FEASIBILITY_TOLERANCE) {
        add_violation(report, "orcamento(" + format_excess(report.cost - users_parameters.budget) + ")");
    }

    // --- restrições de cada dia -----------------------------------------
    std::map<int, int> visits; // local -> quantas vezes aparece na viagem

    for (size_t day_index = 0; day_index < days.size(); ++day_index) {
        const std::vector<int>& day = days[day_index];
        const std::string day_name = "_dia" + std::to_string(day_index + 1);

        // distância: hotel -> locais na ordem -> hotel
        const double distance = day_route_distance(day, places_parameters);
        report.daily_distance.push_back(distance);
        if (distance > users_parameters.total_distance + FEASIBILITY_TOLERANCE) {
            add_violation(report, "distancia" + day_name + "(" +
                                      format_excess(distance - users_parameters.total_distance) + ")");
        }

        // quantidade de locais
        const int places = static_cast<int>(day.size()) - 1;
        report.daily_places.push_back(places);
        if (places > users_parameters.amount_daily_places) {
            add_violation(report, "locais" + day_name + "(" + std::to_string(places) + ">" +
                                      std::to_string(users_parameters.amount_daily_places) + ")");
        }

        // categoria: maior sequência de locais seguidos da mesma categoria
        int longest = 0, current = 0;
        for (size_t i = 1; i < day.size(); ++i) {
            const Category category = places_parameters.category[static_cast<size_t>(day[i])];
            if (category == Category::hotel) {
                add_violation(report, "estrutura(hotel_como_local:" + std::to_string(day[i]) + ")");
            }
            const bool same = i > 1 && category == places_parameters.category[static_cast<size_t>(day[i - 1])];
            current = same ? current + 1 : 1;
            longest = std::max(longest, current);
        }
        report.max_consecutive_category = std::max(report.max_consecutive_category, longest);
        if (longest > users_parameters.category) {
            add_violation(report, "categoria" + day_name + "(" + std::to_string(longest) + ">" +
                                      std::to_string(users_parameters.category) + ")");
        }

        for (size_t i = 1; i < day.size(); ++i) {
            visits[day[i]] += 1;
        }
    }

    // --- repetição na viagem inteira ----------------------------------------
    for (const auto& [place, count] : visits) {
        if (count > 1) {
            add_violation(report, "repetido(" + std::to_string(place) + "x" + std::to_string(count) + ")");
        }
    }

    return report;
}

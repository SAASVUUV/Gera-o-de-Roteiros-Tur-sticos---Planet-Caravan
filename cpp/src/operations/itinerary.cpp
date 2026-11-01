#include "itinerary.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "../model/feasibility.hpp"
#include "../model/objective_function.hpp"

namespace {

// 8432.6 -> "8.433 m"
std::string format_meters(double meters) {
    std::string digits = std::to_string(static_cast<long long>(std::llround(meters)));
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<size_t>(i), ".");
    }
    return digits + " m";
}

// 214 -> "R$ 214", 12.5 -> "R$ 12,50"
std::string format_money(double value) {
    char buffer[64];
    if (value == std::floor(value)) {
        std::snprintf(buffer, sizeof(buffer), "R$ %.0f", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "R$ %.2f", value);
        for (char* c = buffer; *c; ++c) {
            if (*c == '.') *c = ',';
        }
    }
    return buffer;
}

std::string format_rating(double rating) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", rating);
    for (char* c = buffer; *c; ++c) {
        if (*c == '.') *c = ',';
    }
    return buffer;
}

std::string kind(int place, const PlacesParameters& places_parameters) {
    switch (places_parameters.category[static_cast<size_t>(place)]) {
    case Category::hotel:
        return "hotel";
    case Category::restaurant:
        return "restaurante";
    default:
        if (!places_parameters.types.empty() && !places_parameters.types[static_cast<size_t>(place)].empty()) {
            return "atração: " + places_parameters.types[static_cast<size_t>(place)];
        }
        return "atração";
    }
}

// largura visível de um texto UTF-8 (conta caracteres, não bytes)
size_t visible_width(const std::string& text) {
    size_t width = 0;
    for (unsigned char c : text) {
        width += (c & 0xC0) != 0x80;
    }
    return width;
}

std::string pad(const std::string& text, size_t width) {
    const size_t current = visible_width(text);
    return text + std::string(width > current ? width - current : 0, ' ');
}

} // namespace

std::string place_name(int place, const PlacesParameters& places_parameters) {
    if (places_parameters.names.empty()) {
        return "#" + std::to_string(place);
    }
    return places_parameters.names[static_cast<size_t>(place)];
}

std::string compact_itinerary(const std::vector<int>& route, const PlacesParameters& places_parameters) {
    const std::vector<std::vector<int>> days = split_days(route);
    if (days.empty()) {
        return "-";
    }
    std::string text = place_name(days[0][0], places_parameters);
    for (size_t day = 0; day < days.size(); ++day) {
        text += " | Dia " + std::to_string(day + 1) + ": ";
        for (size_t i = 1; i < days[day].size(); ++i) {
            text += (i > 1 ? " > " : "") + place_name(days[day][i], places_parameters);
        }
        if (days[day].size() == 1) {
            text += "(sem locais)";
        }
    }
    return text;
}

std::string detailed_itinerary(const std::vector<int>& route, const PlacesParameters& places_parameters,
                               const UsersParameters& users_parameters) {
    std::ostringstream out;
    const std::vector<std::vector<int>> days = split_days(route);
    if (days.empty()) {
        return "  (rota vazia)\n";
    }

    const int hotel = days[0][0];
    const double hotel_price = places_parameters.price[static_cast<size_t>(hotel)];

    // largura das colunas de nome e tipo
    size_t name_width = 0, kind_width = 0;
    for (const std::vector<int>& day : days) {
        for (size_t i = 1; i < day.size(); ++i) {
            name_width = std::max(name_width, visible_width(place_name(day[i], places_parameters)));
            kind_width = std::max(kind_width, visible_width(kind(day[i], places_parameters)));
        }
    }

    out << "  Hotel: " << place_name(hotel, places_parameters) << " (nota " << format_rating(
               places_parameters.rating[static_cast<size_t>(hotel)])
        << ", " << format_money(hotel_price) << " por diária, ID " << hotel << ")\n";

    double places_cost = 0;
    for (size_t day_index = 0; day_index < days.size(); ++day_index) {
        const std::vector<int>& day = days[day_index];
        const double distance = day_route_distance(day, places_parameters);

        out << "\n  Dia " << day_index + 1 << ": " << day.size() - 1 << " locais, " << format_meters(distance)
            << " a pé (limite " << format_meters(users_parameters.total_distance) << ")\n";
        out << "     sai do hotel\n";

        for (size_t i = 1; i < day.size(); ++i) {
            const int place = day[i];
            const double price = places_parameters.price[static_cast<size_t>(place)];
            places_cost += price;
            out << "     " << pad("+ " + format_meters(places_parameters.distance(day[i - 1], place)), 12) << " "
                << i << ". " << pad(place_name(place, places_parameters), name_width) << "  "
                << pad(kind(place, places_parameters), kind_width) << "  nota "
                << format_rating(places_parameters.rating[static_cast<size_t>(place)]) << "  "
                << pad(format_money(price), 8) << "  ID " << place << "\n";
        }
        out << "     " << pad("+ " + format_meters(places_parameters.distance(day.back(), hotel)), 12)
            << " volta ao hotel\n";
    }

    const double hotel_cost = hotel_price * users_parameters.amount_days;
    out << "\n  Custo: " << format_money(hotel_cost + places_cost) << " de " << format_money(users_parameters.budget)
        << " (hotel " << format_money(hotel_cost) << " por " << users_parameters.amount_days << " diárias + locais "
        << format_money(places_cost) << ")\n";
    out << "  Fitness: " << format_rating(measure_fitness(route, places_parameters.rating)) << " (soma das notas dos locais distintos, hotel incluído)\n";
    return out.str();
}

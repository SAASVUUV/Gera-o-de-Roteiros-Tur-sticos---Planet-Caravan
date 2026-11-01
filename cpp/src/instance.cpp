#include "instance.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

static std::ifstream open_file(const std::string& path, std::ios::openmode mode = std::ios::in) {
    std::ifstream file(path, mode);
    if (!file) {
        throw std::runtime_error("não foi possível abrir " + path +
                                 " (rode antes: python scripts/export_instance.py)");
    }
    return file;
}

static Category parse_category(const std::string& text) {
    if (text == "hotel") return Category::hotel;
    if (text == "restaurant") return Category::restaurant;
    if (text == "attraction") return Category::attraction;
    throw std::runtime_error("categoria desconhecida: " + text);
}

static std::vector<int> read_ids(const std::string& path, size_t size) {
    std::ifstream file = open_file(path);
    std::vector<int> ids;
    int id;
    while (file >> id) {
        if (id < 0 || static_cast<size_t>(id) >= size) {
            throw std::runtime_error("ID inválido em " + path);
        }
        ids.push_back(id);
    }
    return ids;
}

PlacesParameters read_instance(const std::string& directory) {
    PlacesParameters places;

    // places.tsv: id, price, rating, category
    std::ifstream places_file = open_file(directory + "/places.tsv");
    std::string line;
    std::getline(places_file, line); // cabeçalho
    while (std::getline(places_file, line)) {
        if (line.empty()) continue;
        std::istringstream fields(line);
        std::string id, price, rating, category;
        std::getline(fields, id, '\t');
        std::getline(fields, price, '\t');
        std::getline(fields, rating, '\t');
        std::getline(fields, category, '\t');
        if (std::stoul(id) != places.price.size()) {
            throw std::runtime_error("places.tsv fora de ordem no ID " + id);
        }
        // strtod lê de volta exatamente o valor escrito por repr() no Python
        places.price.push_back(std::strtod(price.c_str(), nullptr));
        places.rating.push_back(std::strtod(rating.c_str(), nullptr));
        places.category.push_back(parse_category(category));
    }
    places.size = places.price.size();

    places.hotels = read_ids(directory + "/hotels.txt", places.size);
    places.places_available = read_ids(directory + "/places_available.txt", places.size);
    places.greedy_order = read_ids(directory + "/greedy_order.txt", places.size);
    if (places.greedy_order.size() != places.places_available.size()) {
        throw std::runtime_error("greedy_order.txt não corresponde a places_available.txt "
                                 "(rode de novo: python scripts/export_instance.py)");
    }

    // distance_matrix.bin: n x n float64 little-endian
    std::ifstream matrix_file = open_file(directory + "/distance_matrix.bin", std::ios::binary);
    places.distance_data.resize(places.size * places.size);
    matrix_file.read(reinterpret_cast<char*>(places.distance_data.data()),
                     static_cast<std::streamsize>(places.distance_data.size() * sizeof(double)));
    if (matrix_file.gcount() != static_cast<std::streamsize>(places.distance_data.size() * sizeof(double)) ||
        matrix_file.peek() != std::char_traits<char>::eof()) {
        throw std::runtime_error("distance_matrix.bin não tem " + std::to_string(places.size) + "x" +
                                 std::to_string(places.size) + " valores");
    }

    return places;
}

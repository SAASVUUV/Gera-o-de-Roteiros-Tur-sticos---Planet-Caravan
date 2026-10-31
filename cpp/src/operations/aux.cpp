#include "aux.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "../numpy_compat.hpp"

static std::ofstream open_for_append(const std::string& output_file) {

    const std::filesystem::path parent = std::filesystem::path(output_file).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(output_file, std::ios::app);
    if (!file) {
        throw std::runtime_error("não foi possível escrever em " + output_file);
    }
    return file;
}

void write_comment(const std::string& output_file, const std::string& text) {

    open_for_append(output_file) << "# " << text << '\n';
}

void write_results(const std::string& output_file, const Json& budget, const Json& distance,
                   double time, const std::string& fitness, const std::string& route,
                   const std::string& algorithm) {

    std::ofstream file = open_for_append(output_file);

    // str() de um int do Python e do json.dump() são iguais ("500");
    // strings saem sem aspas, como str() no Python.
    auto to_str = [](const Json& value) {
        return value.is_string() ? value.get<std::string>() : value.dump();
    };

    file << algorithm << '\t' << to_str(budget) << '\t' << to_str(distance) << '\t'
         << numpy_compat::py_float_str(time) << '\t' << fitness << '\t' << route << '\n';
}

Json read_parameters(const std::string& file_parameters) {
    const std::string path = "Parameters/" + file_parameters;
    std::ifstream json_file(path);
    if (!json_file) {
        throw std::runtime_error("não foi possível abrir " + path + " (rode a partir da raiz do repositório)");
    }
    return Json::parse(json_file);
}

std::vector<Json> explode_parameters(const Json& parameters) {
    std::vector<Json> combinations{Json::object()};

    for (const auto& [key, values] : parameters.items()) {
        std::vector<Json> extended;
        for (const Json& partial : combinations) {
            for (const Json& value : values) {
                Json combination = partial;
                combination[key] = value;
                extended.push_back(combination);
            }
        }
        combinations = std::move(extended);
    }

    return combinations;
}

UsersParameters users_parameters_from_json(const Json& combination) {
    UsersParameters users;
    users.budget = combination.at("budget").get<double>();
    users.total_distance = combination.at("total_distance").get<double>();
    users.category = combination.at("category").get<int>();
    users.amount_daily_places = combination.at("amount_daily_places").get<int>();
    users.amount_days = combination.at("amount_days").get<int>();
    return users;
}

GraspParameters grasp_parameters_from_json(const Json& combination) {
    GraspParameters grasp;
    grasp.max_iterations = combination.at("max_iterations").get<int>();
    return grasp;
}

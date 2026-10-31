// GRASP Algorithm

#include "grasp.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <tuple>

#include "../model/objective_function.hpp"
#include "../operations/constructive_operations.hpp"
#include "random_greedy_procedure.hpp"

using numpy_compat::py_at;

bool verify_local_search_changes(RouteParameters& route_parameters,
                                 const UsersParameters& users_parameters,
                                 const PlacesParameters& places_parameters, int old_place,
                                 int new_place, long index, int day) {
    // temos que verificar o budget
    // temos que verificar a distância

    DayParameters& day_parameters = route_parameters.days[static_cast<size_t>(day)];
    const std::vector<int>& route = day_parameters.route;

    const double budget = route_parameters.budget - places_parameters.price[static_cast<size_t>(old_place)] +
                          places_parameters.price[static_cast<size_t>(new_place)];

    if (budget > users_parameters.budget) {

        return false;
    }

    int before, after;

    // quer dizer que é ultimo local a ser visitado naquele dia
    // ORIGINAL: index vem do enumerate(route[1:]), então nunca é igual a
    // len(route) - 1 e este ramo nunca executa
    if (static_cast<long>(route.size()) - 1 == index) {

        // o último local é o hotel
        before = py_at(route, index - 1);
        after = route[0];

    } else {

        // ORIGINAL: o local trocado está na posição index + 1, então "before"
        // é o local duas posições antes (para index = 0, route[-1] é o último
        // local do dia) e "after" é o próprio local trocado
        before = py_at(route, index - 1);
        after = py_at(route, index + 1);
    }

    // tirando a distância gasta visitando aquele local anterior
    // ORIGINAL: este valor é descartado pela linha seguinte
    [[maybe_unused]] double distance = day_parameters.daily_distance -
                                       places_parameters.distance(before, old_place) -
                                       places_parameters.distance(old_place, after);

    // ORIGINAL: soma o trecho novo sem ter subtraído o antigo
    distance = day_parameters.daily_distance + places_parameters.distance(before, new_place) +
               places_parameters.distance(new_place, after);

    if (distance >= users_parameters.total_distance) {

        return false;
    }

    day_parameters.daily_distance = distance;

    // ORIGINAL: sobrescreve a distância diária com o custo; e
    // route_parameters["budget"] nunca é atualizado
    day_parameters.daily_distance = budget;

    return true;
}

std::pair<RouteParameters, double> local_search(RouteParameters route_parameters,
                                                const UsersParameters& users_parameters,
                                                const PlacesParameters& places_parameters,
                                                numpy_compat::RandomState& random) {

    const std::vector<Category>& categories = places_parameters.category;

    const std::vector<double>& ratings = places_parameters.rating;

    std::vector<int> route = generate_route_array(users_parameters, route_parameters);

    for (int day = 0; day < users_parameters.amount_days; ++day) {

        std::vector<int>& day_route = route_parameters.days[static_cast<size_t>(day)].route;

        // for index, place in enumerate(route_parameters[day]["route"][1:])
        const long amount = static_cast<long>(day_route.size()) - 1;

        for (long index = 0; index < amount; ++index) {

            const int place = day_route[static_cast<size_t>(index + 1)];

            // outros locais que vão ser visitados outros dias não podem estar na rota
            std::vector<int> pois = numpy_compat::setdiff1d(places_parameters.places_available, route);

            pois = numpy_compat::setdiff1d(pois, day_route);

            if (ratings[static_cast<size_t>(place)] != 5) {

                // então há espaço de melhoria
                // quais são os locais mais próximos do local atual
                // que possuem mesma categoria, e possuem uma boa nota

                std::vector<int> places_available;
                for (int poi : pois) {
                    if (ratings[static_cast<size_t>(poi)] > ratings[static_cast<size_t>(place)] &&
                        categories[static_cast<size_t>(place)] == categories[static_cast<size_t>(poi)]) {
                        places_available.push_back(poi);
                    }
                }

                std::vector<double> visit_gain;
                visit_gain.reserve(places_available.size());
                for (int poi : places_available) {
                    visit_gain.push_back(places_parameters.distance(place, poi) / ratings[static_cast<size_t>(poi)]);
                }

                if (!places_available.empty()) {

                    const double min_gain = *std::min_element(visit_gain.begin(), visit_gain.end());

                    std::vector<int> places;
                    for (size_t i = 0; i < places_available.size(); ++i) {
                        if (visit_gain[i] == min_gain) {
                            places.push_back(places_available[i]);
                        }
                    }

                    // aqui temos que verificar a viabilidade dessa rota

                    std::vector<size_t> indices;
                    for (size_t i = 0; i < route.size(); ++i) {
                        if (route[i] == day_route[static_cast<size_t>(index + 1)]) {
                            indices.push_back(i);
                        }
                    }

                    const int new_place = random.choice(places);

                    const bool respect_constraints =
                        verify_local_search_changes(route_parameters, users_parameters, places_parameters,
                                                    place, new_place, index, day);

                    // não respeita as restrições
                    if (!respect_constraints) {

                        continue;
                    }

                    for (size_t i : indices) {
                        route[i] = new_place;
                    }

                    // tenho que garantir que nota rota é válida
                    // checagem de restrições
                    day_route[static_cast<size_t>(index + 1)] = new_place;
                }
            }
        }
    }

    route = generate_route_array(users_parameters, route_parameters);

    const double fitness = measure_fitness(route, places_parameters.rating);

    return {std::move(route_parameters), fitness};
}

std::vector<double> inverse_probability(const std::vector<double>& probabilities) {

    // Passo 2: Subtrair cada probabilidade da soma total
    std::vector<double> inverse_proba;
    for (double p : probabilities) {
        inverse_proba.push_back(numpy_compat::sum(probabilities) - p);
    }

    // Passo 3: Normalizar as probabilidades
    std::vector<double> normalized;
    for (double p : inverse_proba) {
        normalized.push_back(p / numpy_compat::sum(inverse_proba));
    }

    return normalized;
}

size_t select_alpha(double fitness, const std::vector<AlphaResults>& alphas,
                    numpy_compat::RandomState& random) {
    // Implementação de acordo com o livro do Talbi
    //
    // z* == fitness
    // Ai == np.mean(alphas[alpha]) --> media do alpha atual
    // qj = z*/Ai
    // qi/ sum Ai
    // pi = qi/sum qj

    std::vector<double> alpha_mean;
    for (const AlphaResults& alpha : alphas) {
        alpha_mean.push_back(numpy_compat::mean(alpha.results));
    }

    std::vector<double> mean_ai;
    for (double mean : alpha_mean) {
        mean_ai.push_back(fitness / mean);
    }

    // novas probabilidades
    const double total = numpy_compat::sum(mean_ai);
    std::vector<double> probability_alpha;
    for (size_t alpha_index = 0; alpha_index < alphas.size(); ++alpha_index) {
        probability_alpha.push_back((fitness / alpha_mean[alpha_index]) / total);
    }

    // dando maior probabilidade para quem o fitness maior
    // a função feita pelo livro do talbi tem como objetivo a minimização
    // a função acima tem por objetivo realizar a probabilidade inversa
    // ou seja, o alpha que antes tinha a menor probabilidade passa a ser maior
    std::vector<size_t> keys(alphas.size());
    for (size_t i = 0; i < keys.size(); ++i) {
        keys[i] = i;
    }

    return random.choice(keys, inverse_probability(probability_alpha));
}

std::vector<int> generate_daily_array_route(const UsersParameters& users_parameters,
                                            const RouteParameters& route_parameters) {

    // ORIGINAL: os dois ramos do if são iguais, e np.array(route, route[0])
    // só copia o array. O resultado é a concatenação das rotas de cada dia,
    // com o hotel no início de cada uma.
    std::vector<int> route;

    for (int day = 0; day < users_parameters.amount_days; ++day) {

        const std::vector<int>& day_route = route_parameters.days[static_cast<size_t>(day)].route;

        route.insert(route.end(), day_route.begin(), day_route.end());
    }

    return route;
}

std::pair<std::vector<int>, double> grasp_heuristic(const PlacesParameters& places_parameters,
                                                    const UsersParameters& users_parameters,
                                                    const GraspParameters& algorithm_parameters,
                                                    numpy_compat::RandomState& random) {

    int iteration = 0;
    double best_fitness = 0;
    std::optional<RouteParameters> best_solution;

    // alphas = {alpha / 10: {"results": np.array([])} for alpha in range(3, 8)}
    std::vector<AlphaResults> alphas;
    for (int alpha = 3; alpha < 8; ++alpha) {
        alphas.push_back({alpha / 10.0, {}});
    }

    size_t alpha_index = 0, count_alpha = 0;

    size_t alpha = 0; // posição do alpha atual em alphas

    while (iteration < algorithm_parameters.max_iterations) {

        if (count_alpha < alphas.size()) {

            alpha = alpha_index;

            alpha_index += 1;

            count_alpha += 1;
        }

        RouteParameters solution =
            random_greedy(places_parameters, users_parameters, alphas[alpha].alpha, random);

        // ORIGINAL: before_ls é calculado e nunca usado. Não consome números
        // aleatórios, então omiti-lo não muda o resultado:
        //   route = generate_route_array(users_parameters, solution)
        //   before_ls = measure_fitness(route, places_parameters.rating)

        double fitness;
        std::tie(solution, fitness) = local_search(std::move(solution), users_parameters, places_parameters, random);

        alphas[alpha].results.push_back(fitness);

        if (count_alpha >= alphas.size()) {
            // durante as 5 primeiras rodadas utilizamos um alpha de cada
            // para poder ter insumo e fazer o calculo
            // ORIGINAL: usa o fitness desta iteração (não o melhor), e o alpha
            // sorteado só vale para a próxima iteração
            alpha = select_alpha(fitness, alphas, random);
        }

        // selectiona o alpha com base na probabilidade

        if (iteration == 0 || fitness > best_fitness) {

            best_fitness = fitness;

            best_solution = solution;
        }

        iteration += 1;
    }

    if (!best_solution.has_value()) {
        // no Python, generate_daily_array_route(None) daria erro
        throw std::invalid_argument("max_iterations must be at least 1");
    }

    return {generate_daily_array_route(users_parameters, *best_solution), best_fitness};
}

// GRASP Algorithm

#include "grasp.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <tuple>

#include "../model/feasibility.hpp"
#include "../model/objective_function.hpp"
#include "../operations/constructive_operations.hpp"
#include "random_greedy_procedure.hpp"
#include "simple_greedy.hpp"


bool verify_local_search_changes(RouteParameters& route_parameters,
                                 const UsersParameters& users_parameters,
                                 const PlacesParameters& places_parameters, int old_place,
                                 int new_place, long position, int day) {
    // temos que verificar o budget
    // temos que verificar a distância
    //
    // CORRIGIDO (viabilidade). No original:
    //   - o índice estava deslocado (usava a posição do local - 1), então os
    //     trechos verificados não eram os que mudam com a troca, e a volta ao
    //     hotel nunca era verificada;
    //   - o trecho antigo não era descontado;
    //   - daily_distance recebia o custo, e o budget nunca era atualizado.
    // Agora `position` é a posição real do local em route, a distância do dia
    // é recalculada com a troca (incluindo a volta ao hotel), e o budget e a
    // distância do dia são atualizados quando a troca é aceita.

    DayParameters& day_parameters = route_parameters.days[static_cast<size_t>(day)];

    const double budget = route_parameters.budget - places_parameters.price[static_cast<size_t>(old_place)] +
                          places_parameters.price[static_cast<size_t>(new_place)];

    if (budget > users_parameters.budget) {

        return false;
    }

    // rota do dia com a troca: hotel -> ... -> novo local -> ... -> hotel
    std::vector<int> new_route = day_parameters.route;
    new_route[static_cast<size_t>(position)] = new_place;

    const double distance = day_route_distance(new_route, places_parameters);

    if (distance > users_parameters.total_distance) {

        return false;
    }

    // daily_distance passa a incluir a volta ao hotel (ver local_search)
    day_parameters.daily_distance = distance;

    route_parameters.budget = budget;

    return true;
}

std::vector<int> order_candidates(const std::vector<int>& candidates, int place, int before, int after,
                                  const PlacesParameters& places_parameters) {
    // Δdistância(N) = d(A→N) + d(N→C) − d(A→P) − d(P→C)
    //   A = local anterior a P, C = próximo (ou o hotel, se P é o último do dia)
    // ganho(N) = nota(N) − nota(P), sempre > 0
    //
    // Grupo 1 (Δ <= 0, a troca não aumenta o dia): maior ganho primeiro;
    //   empate: o que mais economiza distância. Aqui a distância não limita,
    //   então o que importa é o fitness. (A razão Δ/ganho colocaria os de
    //   menor ganho na frente, porque Δ é negativo.)
    // Grupo 2 (Δ > 0): menor Δ/ganho primeiro, ou seja, quem anda menos
    //   por ponto de nota ganho.
    // Empates restantes: menor ID (candidates já vem ordenado), sem sorteio.
    struct Candidate {
        int place;
        double delta;
        double gain;
    };

    const double removed = places_parameters.distance(before, place) + places_parameters.distance(place, after);
    const double rating = places_parameters.rating[static_cast<size_t>(place)];

    std::vector<Candidate> group_1, group_2;
    for (int candidate : candidates) {
        const double delta = places_parameters.distance(before, candidate) +
                             places_parameters.distance(candidate, after) - removed;
        const double gain = places_parameters.rating[static_cast<size_t>(candidate)] - rating;
        (delta <= 0 ? group_1 : group_2).push_back({candidate, delta, gain});
    }

    std::stable_sort(group_1.begin(), group_1.end(), [](const Candidate& a, const Candidate& b) {
        if (a.gain != b.gain) return a.gain > b.gain;
        return a.delta < b.delta;
    });
    std::stable_sort(group_2.begin(), group_2.end(), [](const Candidate& a, const Candidate& b) {
        return a.delta / a.gain < b.delta / b.gain;
    });

    std::vector<int> ordered;
    ordered.reserve(candidates.size());
    for (const Candidate& candidate : group_1) ordered.push_back(candidate.place);
    for (const Candidate& candidate : group_2) ordered.push_back(candidate.place);
    return ordered;
}

std::pair<RouteParameters, double> local_search(RouteParameters route_parameters,
                                                const UsersParameters& users_parameters,
                                                const PlacesParameters& places_parameters,
                                                [[maybe_unused]] numpy_compat::RandomState& random) {
    // GRASP_v2: a busca local não sorteia mais nada (a v1 sorteava entre
    // candidatos empatados). O gerador continua no parâmetro para manter a
    // interface.

    const std::vector<Category>& categories = places_parameters.category;

    const std::vector<double>& ratings = places_parameters.rating;

    std::vector<int> route = generate_route_array(users_parameters, route_parameters);

    for (int day = 0; day < users_parameters.amount_days; ++day) {

        std::vector<int>& day_route = route_parameters.days[static_cast<size_t>(day)].route;

        // CORRIGIDO: na construção, daily_distance não inclui a volta ao
        // hotel. Na busca local ela passa a ser a distância real do dia.
        route_parameters.days[static_cast<size_t>(day)].daily_distance =
            day_route_distance(day_route, places_parameters);

        // for index, place in enumerate(route_parameters[day]["route"][1:])
        const long amount = static_cast<long>(day_route.size()) - 1;

        for (long index = 0; index < amount; ++index) {

            const int place = day_route[static_cast<size_t>(index + 1)];

            // outros locais que vão ser visitados outros dias não podem estar na rota
            std::vector<int> pois = numpy_compat::setdiff1d(places_parameters.places_available, route);

            pois = numpy_compat::setdiff1d(pois, day_route);

            if (ratings[static_cast<size_t>(place)] != 5) {

                // então há espaço de melhoria
                // candidatos: mesma categoria e nota maior que a do local atual

                std::vector<int> places_available;
                for (int poi : pois) {
                    if (ratings[static_cast<size_t>(poi)] > ratings[static_cast<size_t>(place)] &&
                        categories[static_cast<size_t>(place)] == categories[static_cast<size_t>(poi)]) {
                        places_available.push_back(poi);
                    }
                }

                if (!places_available.empty()) {

                    // GRASP_v2: os candidatos são ordenados pelo efeito da troca no
                    // trajeto, e testados em ordem até um caber (antes: só o mais
                    // próximo de P era testado). Ver order_candidates.
                    const long position = index + 1;
                    const int before = day_route[static_cast<size_t>(position - 1)];
                    const int after = position + 1 < static_cast<long>(day_route.size())
                                          ? day_route[static_cast<size_t>(position + 1)]
                                          : day_route[0]; // último local do dia: volta ao hotel

                    const std::vector<int> ordered =
                        order_candidates(places_available, place, before, after, places_parameters);

                    // aqui temos que verificar a viabilidade dessa rota
                    for (int new_place : ordered) {

                        // CORRIGIDO: passa a posição real do local (index + 1), e não index
                        const bool respect_constraints = verify_local_search_changes(
                            route_parameters, users_parameters, places_parameters, place, new_place, position, day);

                        // não respeita as restrições: tenta o próximo candidato
                        if (!respect_constraints) {

                            continue;
                        }

                        for (int& visited : route) {
                            if (visited == place) {
                                visited = new_place;
                            }
                        }

                        day_route[static_cast<size_t>(position)] = new_place;

                        break;
                    }
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

GraspResult grasp_heuristic(const PlacesParameters& places_parameters, const UsersParameters& users_parameters,
                            const GraspParameters& algorithm_parameters, numpy_compat::RandomState& random) {

    int iteration = 0;
    double best_fitness = 0;
    std::optional<RouteParameters> best_solution;

    // GRASP_v3: o roteiro do Greedy começa como melhor solução (semente). O
    // Greedy é determinístico e não sorteia nada, então as iterações abaixo
    // são exatamente as da v2: o resultado é o da v2 quando ela alcança o
    // Greedy, e o do Greedy quando não alcança.
    // Se nenhum hotel nota 5 couber no orçamento, o Greedy falha (como no
    // Python) e o GRASP segue sem semente.
    std::optional<std::pair<std::vector<int>, double>> greedy;
    try {
        greedy = construct_greedy_route(places_parameters, users_parameters);
    } catch (const GreedyHotelNotFound&) {
    }

    std::optional<SolutionOrigin> origin; // de onde veio a melhor solução até agora
    if (greedy.has_value()) {
        best_fitness = greedy->second;
        origin = SolutionOrigin::greedy;
    }

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

        // GRASP_v3: uma solução do GRASP substitui a semente gulosa se for
        // maior OU IGUAL a ela; entre soluções do GRASP vale a regra original
        // (só substitui se for maior; a primeira iteração sempre entra quando
        // não há semente).
        bool replace;
        if (!origin.has_value()) {
            replace = true; // iteration == 0 sem semente
        } else if (*origin == SolutionOrigin::greedy) {
            replace = fitness >= best_fitness;
        } else {
            replace = fitness > best_fitness;
        }

        if (replace) {

            best_fitness = fitness;

            best_solution = solution;

            origin = SolutionOrigin::grasp;
        }

        iteration += 1;
    }

    if (!origin.has_value()) {
        // sem semente e sem iterações; no Python, generate_daily_array_route(None) daria erro
        throw std::invalid_argument("max_iterations must be at least 1");
    }

    if (*origin == SolutionOrigin::greedy) {
        return {greedy->first, best_fitness, SolutionOrigin::greedy};
    }

    return {generate_daily_array_route(users_parameters, *best_solution), best_fitness, SolutionOrigin::grasp};
}

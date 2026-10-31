import numpy as np


def verify_constructive_heuristic_constraints(
    route, new_place, places_parameters, users_parameters, route_parameters, current_day
):

    distance_matrix = places_parameters["distance_matrix"]

    total_cost = route_parameters["budget"] + places_parameters["price"][new_place]

    if len(route) + 1 > users_parameters["amount_daily_places"]:

        return False, route_parameters

    if total_cost > users_parameters["budget"]:

        return False, route_parameters

    back_to_hotel = (
        distance_matrix[route[-1]][new_place] + distance_matrix[new_place][route[0]]
    )

    if (
        route_parameters[current_day]["daily_distance"] + back_to_hotel
        > users_parameters["total_distance"]
    ):

        return False, route_parameters

    new_place_category = places_parameters["category"][new_place]

    if (
        route_parameters[current_day][new_place_category] + 1
        > users_parameters["category"]
    ):

        return False, route_parameters

    if new_place in route:

        return False, route_parameters

    # atualizando os parâmetros da rota
    route_parameters[current_day]["daily_distance"] += distance_matrix[route[-1]][
        new_place
    ]

    route_parameters[current_day][new_place_category] += 1

    route_parameters["budget"] = total_cost

    return True, route_parameters


def generate_route_array(users_parameters, route_parameters):

    route = np.array([], dtype=int)

    for day in range(0, users_parameters["amount_days"]):

        if day == 0:

            route = np.append(route, route_parameters[day]["route"])

        else:

            route = np.append(route, route_parameters[day]["route"][1:])

    return route

"""

	Main code

"""

import random

import pandas as pd

import numpy as np

import opt_call


def read_distance_matrix(city):

    distance_matrix = pd.read_table("Input/" + city + "_distance_matrix.txt", sep="\t")

    distance_matrix.set_index("Place Names", inplace=True)

    distance_matrix.index = list(map(lambda index: str(index), distance_matrix.index))

    distance_matrix.columns = list(
        map(lambda column: str(column), distance_matrix.columns)
    )

    distance_matrix.index = distance_matrix.index.str.replace("htmlreviews", "")
    distance_matrix.index = distance_matrix.index.str.replace("html", "")
    distance_matrix.columns = distance_matrix.columns.str.replace("htmlreviews", "")
    distance_matrix.columns = distance_matrix.columns.str.replace("html", "")

    columns = []

    for column in distance_matrix.columns:

        if "/Hotel_Review" in column:

            columns.append(column.replace("/Hotel", "/0AHotel_"))

        else:

            columns.append(column)

    distance_matrix.columns = columns

    columns = []

    for column in distance_matrix.index:

        if "/Hotel_Review" in column:

            columns.append(column.replace("/Hotel", "/0AHotel_"))

        else:

            columns.append(column)

    distance_matrix.index = columns

    return distance_matrix[sorted(distance_matrix.columns)][
        sorted(distance_matrix.columns)
    ]


if __name__ == "__main__":

    np.random.seed(1)

    city = "RJ"

    df = pd.read_table("Input/" + city + ".csv", sep=";")

    df.columns = df.columns.str.title()

    df = df.rename(columns={"Category": "General Category", "Rating": "Mean Sentiment"})

    df["Original Name"] = df["Name"]

    df["Name"] = df["Local Url"].str.replace("htmlreviews", "")
    df["Name"] = df["Local Url"].str.replace("html", "")

    distance_matrix = read_distance_matrix(city)

    ## temos que garantir que os hotéis vem primeiro
    df.loc[df["General Category"] == "hotels", "General Category"] = "hotel"

    df.loc[
        ~df["General Category"].isin(["hotel", "restaurant"]), "General Category"
    ] = "attraction"

    df.loc[df["General Category"] == "hotel", "Name"] = df[
        df["General Category"] == "hotel"
    ]["Name"].str.replace("/Hotel", "/0AHotel_")

    df = pd.concat(
        [df[df["General Category"] == "hotel"], df[df["General Category"] != "hotel"]]
    )

    df = df.reset_index(drop=True)

    # agora temos que garantir que esses locais estejam primeiro
    ## As gambiarras nessa parte são para garantir que os hotéis venham primeiro
    place_distance_matrix = np.sort(
        np.intersect1d(df["Name"].unique(), distance_matrix.columns.unique())
    )

    distance_matrix = distance_matrix[
        distance_matrix.index.isin(place_distance_matrix)
    ][place_distance_matrix]

    distance_matrix = distance_matrix[place_distance_matrix][place_distance_matrix]

    distance_matrix = distance_matrix.sort_index()

    codes = {
        original_name: id_place
        for id_place, original_name in enumerate(place_distance_matrix)
    }

    df = df[
        (df["Name"].isin(distance_matrix.columns))
        & (df["Name"].isin(distance_matrix.index))
    ]

    df["Name"] = df["Name"].map(codes).astype(int)

    df = df.reset_index(drop=True)

    distance_matrix.index = list(map(lambda index: codes[index], distance_matrix.index))

    distance_matrix.columns = list(
        map(lambda column: codes[column], distance_matrix.columns)
    )

    distance_matrix = distance_matrix.values

    # df['Activity Time'] = df.apply(lambda row: random.randint(row['Time Min'], row['Time Max']), axis=1)

    df.loc[df["Mean Sentiment"] == 0, "Mean Sentiment"] = 1

    if "Working Start" in df.columns:

        df["Working Start"] = pd.to_datetime(
            df["Working Start"], format="%d/%m/%Y %I:%M %p"
        )

        df["Working End"] = pd.to_datetime(
            df["Working End"], format="%d/%m/%Y %I:%M %p"
        )

    opt_call.generate_experiments(df, distance_matrix)

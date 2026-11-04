"""
Builds Input/<CITY>_distance_matrix.txt from Input/<CITY>.csv.

This is a stand-in for the original matrix, which was never committed to the
repo. The paper says distances came from OpenStreetMap, so the default method
uses walking distances (in meters) on the OSM pedestrian network. The original
file may differ in details (OSM data changes over time, snapping rules, etc.).

Data cleaning:
  - Some coordinates in the CSV were stored scaled by 1000 (e.g. -22979 instead
    of -22.979). They are divided by 1000 until they are valid degrees.
  - Places outside the municipality are left out: neighboring cities and
    geocoding errors. The boundary is IBGE's official one, saved in
    Input/<city>_boundary.geojson (downloaded if missing). Places up to
    --boundary-tolerance-m beyond it are kept, because the boundary follows a
    simplified coastline and waterfront places can fall just outside.
    main.py keeps only the places present in the matrix, so this is safe.

Usage (from the repo root):
  # Offline, from an OSM extract file (recommended: no busy public servers)
  wget -P Input https://download.geofabrik.de/south-america/brazil/sudeste-latest.osm.pbf
  python scripts/build_distance_matrix.py --pbf Input/sudeste-latest.osm.pbf

  # Online, downloading the street network from the Overpass API
  python scripts/build_distance_matrix.py

  # Straight-line distances, no OSM at all
  python scripts/build_distance_matrix.py --method haversine

Dependencies:
  --pbf:     numpy, pandas, scipy, osmium     (pip install osmium)
  Overpass:  numpy, pandas, scipy, osmnx      (pip install osmnx)
  haversine: numpy, pandas
"""

import argparse
import time
from collections import Counter

import numpy as np
import pandas as pd

EARTH_RADIUS_M = 6_371_000

# IBGE municipality code of each city, used to download its official boundary
# into Input/<city>_boundary.geojson when that file is missing.
CITY_IBGE_CODE = {"RJ": "3304557"}
IBGE_BOUNDARY_URL = (
    "https://servicodados.ibge.gov.br/api/v3/malhas/municipios/{code}"
    "?formato=application/vnd.geo%2Bjson&qualidade=maxima"
)
# Cities outside Brazil: the boundary comes from OSM (Nominatim) via osmnx.
CITY_OSM_QUERY = {
    "NOLA": "New Orleans, Louisiana, USA",
    "NASH": "Nashville, Tennessee, USA",  # Nashville-Davidson (condado inteiro)
    "SBA": "Santa Barbara, California, USA",
}

OVERPASS_SERVERS = [
    "https://overpass-api.de/api",
    "https://overpass.kumi.systems/api",
    "https://overpass.private.coffee/api",
    "https://maps.mail.ru/osm/tools/overpass/api",
]

# Same rules as osmnx's "walk" network type (substring matches, like its
# Overpass regexes): a way is walkable if it has a highway tag and none of
# these tag values contain the listed text.
WALK_EXCLUDE = {
    "highway": ["abandoned", "bus_guideway", "construction", "cycleway", "motor",
                "no", "planned", "platform", "proposed", "raceway", "razed"],
    "area": ["yes"],
    "access": ["private"],
    "foot": ["no"],
    "service": ["private"],
}


def fix_scaled(value):
    value = float(value)
    while abs(value) > 180:
        value /= 1000
    return value


def load_boundary(city):
    """Rings (lists of [lon, lat]) of the municipality's polygons."""
    import json
    import os

    path = f"Input/{city}_boundary.geojson"
    if not os.path.exists(path):
        if city in CITY_IBGE_CODE:
            import urllib.request

            url = IBGE_BOUNDARY_URL.format(code=CITY_IBGE_CODE[city])
            print(f"Downloading municipality boundary from IBGE to {path}")
            urllib.request.urlretrieve(url, path)
        elif city in CITY_OSM_QUERY:
            download_osm_boundary(CITY_OSM_QUERY[city], path)
        else:
            raise SystemExit(f"Missing {path}: add the city to CITY_IBGE_CODE "
                             "or CITY_OSM_QUERY, or provide the GeoJSON file.")

    with open(path, encoding="utf-8") as file:
        geometry = json.load(file)["features"][0]["geometry"]
    polygons = geometry["coordinates"]
    if geometry["type"] == "Polygon":
        polygons = [polygons]
    return [ring for polygon in polygons for ring in polygon]


def download_osm_boundary(query, path):
    """Saves the OSM boundary of `query` in the same GeoJSON shape as IBGE's."""
    import json

    try:
        import osmnx as ox
    except ImportError:
        raise SystemExit("osmnx is not installed. Run `pip install osmnx`.")

    print(f"Downloading boundary of {query!r} from OSM to {path}")
    geometry = ox.geocode_to_gdf(query).geometry.iloc[0].__geo_interface__
    feature = {"type": "Feature", "properties": {"query": query},
               "geometry": json.loads(json.dumps(geometry))}  # tuples -> lists
    with open(path, "w", encoding="utf-8") as file:
        json.dump({"type": "FeatureCollection", "features": [feature]}, file)


def inside_boundary(lat, lon, rings, tolerance_m):
    """True for points inside the rings (even-odd rule) or near their edge."""
    lat0 = lat.mean()
    points = to_local_meters(lat, lon, lat0)
    x, y = points[:, :1], points[:, 1:]
    inside = np.zeros(len(points), dtype=bool)
    edge_distance = np.full(len(points), np.inf)

    for ring in rings:
        ring = np.array(ring)
        vertices = to_local_meters(ring[:, 1], ring[:, 0], lat0)
        a, b = vertices[:-1], vertices[1:]

        # Count crossings of a ray going right from each point.
        crosses = (a[:, 1] > y) != (b[:, 1] > y)
        dy = np.where(b[:, 1] == a[:, 1], 1e-12, b[:, 1] - a[:, 1])
        x_cross = a[:, 0] + (y - a[:, 1]) * (b[:, 0] - a[:, 0]) / dy
        inside ^= (crosses & (x < x_cross)).sum(axis=1) % 2 == 1

        # Distance to the nearest edge of the ring.
        ab = b - a
        t = ((points[:, None] - a) * ab).sum(-1) / np.maximum((ab * ab).sum(-1), 1e-12)
        nearest = a + np.clip(t, 0, 1)[..., None] * ab
        distance = np.linalg.norm(points[:, None] - nearest, axis=-1).min(axis=1)
        edge_distance = np.minimum(edge_distance, distance)

    return inside | (edge_distance <= tolerance_m), edge_distance


def load_places(city, tolerance_m):
    df = pd.read_table(f"Input/{city}.csv", sep=";")
    df["latitude"] = df["latitude"].map(fix_scaled)
    df["longitude"] = df["longitude"].map(fix_scaled)

    keep, edge_distance = inside_boundary(
        df["latitude"].to_numpy(), df["longitude"].to_numpy(),
        load_boundary(city), tolerance_m,
    )
    if (~keep).any():
        print(f"Leaving out {(~keep).sum()} places outside the {city} municipality "
              f"(more than {tolerance_m:.0f} m beyond its boundary):")
        for i in np.flatnonzero(~keep)[np.argsort(edge_distance[~keep])]:
            row = df.iloc[i]
            print(f"  {edge_distance[i] / 1000:8.1f} km  {row['name']}")

    return df[keep].reset_index(drop=True)


def to_local_meters(lat, lon, lat0):
    """Equirectangular projection, accurate enough at city scale."""
    x = np.radians(lon) * EARTH_RADIUS_M * np.cos(np.radians(lat0))
    y = np.radians(lat) * EARTH_RADIUS_M
    return np.column_stack([x, y])


def haversine(lat1, lon1, lat2, lon2):
    lat1, lon1, lat2, lon2 = map(np.radians, (lat1, lon1, lat2, lon2))
    a = (
        np.sin((lat2 - lat1) / 2) ** 2
        + np.cos(lat1) * np.cos(lat2) * np.sin((lon2 - lon1) / 2) ** 2
    )
    return 2 * EARTH_RADIUS_M * np.arcsin(np.sqrt(a))


def haversine_matrix(lat, lon):
    return haversine(lat[:, None], lon[:, None], lat[None, :], lon[None, :])


def study_area(lat, lon, buffer_deg):
    """(south, north, west, east) around the places, plus a margin."""
    return (lat.min() - buffer_deg, lat.max() + buffer_deg,
            lon.min() - buffer_deg, lon.max() + buffer_deg)


# --- Street network sources -------------------------------------------------
# Each returns (node_lat, node_lon, edge_from, edge_to, edge_length), where
# edges index into the node arrays and lengths are in meters.


def network_from_pbf(path, area):
    try:
        import osmium
    except ImportError:
        raise SystemExit("osmium is not installed. Run `pip install osmium`.")

    south, north, west, east = area

    def walkable(tags):
        if "highway" not in tags:
            return False
        for key, excluded in WALK_EXCLUDE.items():
            value = tags.get(key)
            if value is not None and any(text in value for text in excluded):
                return False
        return True

    class WalkWays(osmium.SimpleHandler):
        def __init__(self):
            super().__init__()
            self.ways = []

        def way(self, way):
            if not walkable(way.tags):
                return
            try:
                refs = [node.ref for node in way.nodes]
                lats = [node.lat for node in way.nodes]
                lons = [node.lon for node in way.nodes]
            except osmium.InvalidLocationError:
                return  # way crosses the edge of the extract
            if len(refs) < 2:
                return
            if not any(south <= y <= north and west <= x <= east
                       for y, x in zip(lats, lons)):
                return
            self.ways.append((refs, np.array(lats), np.array(lons)))

    print(f"Reading walkable ways from {path} (a large file takes several minutes)...")
    start = time.time()
    handler = WalkWays()
    # The node location index of a state-sized extract takes GBs of RAM, so
    # keep it in a temporary file on disk next to the extract instead.
    import os
    import tempfile

    index_dir = tempfile.mkdtemp(dir=os.path.dirname(os.path.abspath(path)))
    try:
        index_file = os.path.join(index_dir, "nodes.idx")
        handler.apply_file(path, locations=True, idx=f"sparse_file_array,{index_file}")
    finally:
        for name in os.listdir(index_dir):
            os.remove(os.path.join(index_dir, name))
        os.rmdir(index_dir)
    print(f"  {len(handler.ways)} ways in the study area ({time.time() - start:.0f}s)")
    if not handler.ways:
        raise SystemExit("No walkable ways found: does the file cover the city?")

    # Keep only intersections and way ends as graph nodes; the points in
    # between just add to the length of the edge (like osmnx's simplify).
    uses = Counter()
    for refs, _, _ in handler.ways:
        uses.update(refs)
        uses[refs[0]] += 2
        uses[refs[-1]] += 2

    position, node_lat, node_lon = {}, [], []

    def node_index(ref, y, x):
        if ref not in position:
            position[ref] = len(node_lat)
            node_lat.append(y)
            node_lon.append(x)
        return position[ref]

    edge_from, edge_to, edge_length = [], [], []
    for refs, lats, lons in handler.ways:
        segments = haversine(lats[:-1], lons[:-1], lats[1:], lons[1:])
        begin, length = 0, 0.0
        for i in range(1, len(refs)):
            length += segments[i - 1]
            if uses[refs[i]] >= 2:
                edge_from.append(node_index(refs[begin], lats[begin], lons[begin]))
                edge_to.append(node_index(refs[i], lats[i], lons[i]))
                edge_length.append(length)
                begin, length = i, 0.0

    return (np.array(node_lat), np.array(node_lon),
            edge_from, edge_to, edge_length)


def network_from_overpass(area, overpass_url, attempts):
    try:
        import osmnx as ox
    except ImportError:
        raise SystemExit(
            "osmnx is not installed. Run `pip install osmnx` "
            "(or use --pbf / --method haversine)."
        )

    south, north, west, east = area
    print(f"Downloading OSM walk network for bbox N={north:.3f} S={south:.3f} "
          f"E={east:.3f} W={west:.3f} (this can take a while; osmnx caches it in ./cache)")

    # Many small queries instead of a few big ones: busy servers accept them
    # more easily, and each finished piece is cached, so retries resume.
    ox.settings.max_query_area_size = 200 * 1000 * 1000  # 200 km² per query
    ox.settings.requests_timeout = 600

    servers = [overpass_url] if overpass_url else OVERPASS_SERVERS
    wait = 30
    for attempt in range(1, attempts + 1):
        server = servers[(attempt - 1) % len(servers)]
        ox.settings.overpass_url = server  # osmnx >= 1.9
        ox.settings.overpass_endpoint = server  # older osmnx
        try:
            if int(ox.__version__.split(".")[0]) >= 2:
                graph = ox.graph_from_bbox(
                    (west, south, east, north), network_type="walk"
                )
            else:
                graph = ox.graph_from_bbox(
                    north, south, east, west, network_type="walk"
                )
            break
        except Exception as error:  # server busy, timeout, bad gateway...
            first_line = str(error).strip().splitlines()[0] if str(error) else error
            print(f"Attempt {attempt}/{attempts} on {server} failed: {first_line}")
            if attempt == attempts:
                raise SystemExit(
                    "Could not download OSM data. Try again later, pass "
                    "--overpass-url, or use --pbf / --method haversine."
                )
            print(f"Retrying in {wait}s...")
            time.sleep(wait)
            wait = min(wait * 2, 600)

    node_ids = list(graph.nodes)
    position = {node: i for i, node in enumerate(node_ids)}
    node_lat = np.array([graph.nodes[n]["y"] for n in node_ids])
    node_lon = np.array([graph.nodes[n]["x"] for n in node_ids])
    edge_from, edge_to, edge_length = [], [], []
    for u, v, length in graph.edges(data="length"):
        edge_from.append(position[u])
        edge_to.append(position[v])
        edge_length.append(length)
    return node_lat, node_lon, edge_from, edge_to, edge_length


# --- Distances on the network -----------------------------------------------


def adjacency_matrix(edge_from, edge_to, edge_length, n):
    """Two-way walking graph, keeping the shortest of parallel edges."""
    from scipy.sparse import csr_matrix

    edges = pd.DataFrame({
        "r": np.concatenate([edge_from, edge_to]),
        "c": np.concatenate([edge_to, edge_from]),
        "w": np.concatenate([edge_length, edge_length]),
    })
    edges = edges[edges.r != edges.c].groupby(["r", "c"], as_index=False)["w"].min()
    return csr_matrix((edges.w.to_numpy(), (edges.r, edges.c)), shape=(n, n))


def network_matrix(lat, lon, names, network, chunk_size, far_snap_m=1000):
    from scipy.sparse.csgraph import connected_components, dijkstra
    from scipy.spatial import cKDTree

    node_lat, node_lon, edge_from, edge_to, edge_length = network
    adjacency = adjacency_matrix(edge_from, edge_to, edge_length, len(node_lat))
    print(f"Graph: {len(node_lat)} nodes, {adjacency.nnz} directed edges")

    # Snap only to the largest connected component so every pair is reachable.
    _, labels = connected_components(adjacency, directed=False)
    main_component = np.bincount(labels).argmax()
    candidates = np.flatnonzero(labels == main_component)

    lat0 = lat.mean()
    tree = cKDTree(to_local_meters(node_lat[candidates], node_lon[candidates], lat0))
    snap_distance, nearest = tree.query(to_local_meters(lat, lon, lat0))
    place_nodes = candidates[nearest]
    print(f"Snap distance place -> street (m): median {np.median(snap_distance):.0f}, "
          f"max {snap_distance.max():.0f}")
    far = np.flatnonzero(snap_distance > far_snap_m)
    if len(far):
        print(f"Warning: {len(far)} places are more than {far_snap_m:.0f} m from "
              "any street in the network (bad coordinates, or the map does not "
              "cover them). Their distances include that straight-line walk:")
        for i in far[np.argsort(-snap_distance[far])]:
            print(f"  {snap_distance[i]:7.0f} m  {names[i]}")

    unique_nodes, place_to_unique = np.unique(place_nodes, return_inverse=True)
    distances = np.empty((len(unique_nodes), len(unique_nodes)))
    start = time.time()
    for begin in range(0, len(unique_nodes), chunk_size):
        sources = unique_nodes[begin : begin + chunk_size]
        dist = dijkstra(adjacency, directed=True, indices=sources)
        distances[begin : begin + len(sources)] = dist[:, unique_nodes]
        done = begin + len(sources)
        print(f"  shortest paths {done}/{len(unique_nodes)} "
              f"({time.time() - start:.0f}s)", end="\r")
    print()

    matrix = distances[np.ix_(place_to_unique, place_to_unique)]
    # Add the walk from each place to its street node at both ends.
    matrix = matrix + snap_distance[:, None] + snap_distance[None, :]
    np.fill_diagonal(matrix, 0)
    return matrix


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[1])
    parser.add_argument("--city", default="RJ")
    parser.add_argument("--method", choices=["osm", "haversine"], default="osm")
    parser.add_argument("--pbf",
                        help="read the street network from this OSM extract "
                             "(.osm.pbf) instead of the Overpass API")
    parser.add_argument("--output", help="default: Input/<city>_distance_matrix.txt")
    parser.add_argument("--boundary-tolerance-m", type=float, default=100,
                        help="keep places up to this far outside the "
                             "municipality boundary (simplified coastline)")
    parser.add_argument("--buffer-deg", type=float, default=0.01,
                        help="margin around the places when loading OSM data")
    parser.add_argument("--chunk-size", type=int, default=64,
                        help="Dijkstra sources per batch (memory vs. speed)")
    parser.add_argument("--overpass-url",
                        help="use only this Overpass server (default: rotate "
                             "between public servers)")
    parser.add_argument("--attempts", type=int, default=8,
                        help="Overpass download attempts before giving up")
    args = parser.parse_args()

    if args.method == "osm" and args.pbf:
        import os

        if not os.path.exists(args.pbf):
            raise SystemExit(
                f"{args.pbf} not found. Download it first, e.g.:\n  wget -P Input "
                "https://download.geofabrik.de/south-america/brazil/sudeste-latest.osm.pbf"
            )

    places = load_places(args.city, args.boundary_tolerance_m)
    lat = places["latitude"].to_numpy()
    lon = places["longitude"].to_numpy()
    source = args.pbf if args.method == "osm" and args.pbf else args.method
    print(f"{len(places)} places, distances from: {source}")

    if args.method == "osm":
        area = study_area(lat, lon, args.buffer_deg)
        if args.pbf:
            network = network_from_pbf(args.pbf, area)
        else:
            network = network_from_overpass(area, args.overpass_url, args.attempts)
        matrix = network_matrix(
            lat, lon, places["name"].tolist(), network, args.chunk_size
        )
    else:
        matrix = haversine_matrix(lat, lon)

    names = places["local url"].tolist()
    output = args.output or f"Input/{args.city}_distance_matrix.txt"
    table = pd.DataFrame(matrix, index=names, columns=names)
    table.index.name = "Place Names"
    table.to_csv(output, sep="\t", float_format="%.1f")
    print(f"Saved {matrix.shape[0]}x{matrix.shape[1]} matrix (meters) to {output}")


if __name__ == "__main__":
    main()

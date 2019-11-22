import tomviz.operators
import tomviz.utils

import numpy as np
from scipy.sparse import csr_matrix
from scipy.sparse.csgraph import dijkstra

class DistanceMethod:
    Eucledian = 0
    CityBlock = 1
    ChessBoard = 2

class PropagationDirection:
    Xpos = 0
    Xneg = 1
    Ypos = 2
    Yneg = 3
    Zpos = 4
    Zneg = 5

class OperatorStages:
    GraphGeneration = 0
    GraphTraversal = 1

def coord_iterator(extent):
    if len(extent) == 1:
        for i in range(extent[0][0], extent[0][1]):
            yield (i, )
    elif len(extent) > 1:
        for i in range(extent[0][0], extent[0][1]):
            for c in coord_iterator(extent[1:]):
                yield (i, ) + c

def neighbor_iterator(ndim):
    for c in coord_iterator(((-1, 2),) * ndim):
        if any(c):
            yield c

def get_distance_function(method):
    if method == DistanceMethod.Eucledian:
        import math

        def distance_fn(vec):
            return math.sqrt(sum(x * x for x in vec))
        
        return distance_fn
    
    elif method == DistanceMethod.ChessBoard:

        def distance_fn(vec):
            return 1
        
        return distance_fn
    
    elif method == DistanceMethod.CityBlock:

        def distance_fn(vec):
            if sum(abs(x) for x in vec) > 1:
                return None
            else:
                return 1
        
        return distance_fn

    else:
        raise Exception("Unknown distance method %s" % method)

def volume_to_graph(volume, phase, method, update_progress=None):
    invalid_node_idx = -1
    # The distance function
    distance = get_distance_function(method)
    # Map voxel coordinate to node index (array form, to easily slice when doing analysis. We can do without if memory becomes an issue)
    node_map_array = np.empty(shape=volume.shape, dtype=np.int32)
    node_map_array.fill(invalid_node_idx)
    # Map voxel coordinate to node index
    node_map = {}
    # Map node index to voxel coordinate
    inv_node_map = {}
    # Weighted (i.e. distance) connections between nodes
    edges = {}
    # Edges between auxiliary face nodes and actual nodes on the first slice (x+, x-, y+, y-, z+, z-)
    aux_edges = {}

    # Reserve node_idx 0 to 3 for the auxiliary nodes at the beginning/end of the x/y direction
    # In the propagation this are the distances we are calculating from
    node_idx = volume.ndim * 2

    # Map node indices to positions in the volume
    extent = tuple((0, s) for s in volume.shape)
    for coord in coord_iterator(extent):
        if volume[coord] == phase:
            node_map_array[coord] = node_idx
            inv_node_map[node_idx] = coord
            node_map[coord] = node_idx
            node_idx += 1

    # Find edges between the nodes
    
    delta_coordinates = [d for d in neighbor_iterator(volume.ndim)]
    delta_distances = [distance(delta_coord) for delta_coord in delta_coordinates]

    UPDATE_EVERY = 10000
    n_nodes = len(node_map)

    for i, (node_idx, node_coord) in enumerate(inv_node_map.items()):
        if i % UPDATE_EVERY == 0 and update_progress is not None:
            update_progress(i / n_nodes)

        for delta_coord, delta_distance in zip(delta_coordinates, delta_distances):
            neighbor_coord = tuple(c + d for c, d in zip(node_coord, delta_coord))
            neighbor_idx = node_map.get(neighbor_coord)

            if neighbor_idx is None:
                continue

            if delta_distance is not None:
                edges[(node_idx, neighbor_idx)] = delta_distance

    # Add edges between aux nodes at the faces of the volume
    for i in range(volume.ndim):
        for j in range(2):
            node_idx = i * 2 + j
            face_slice = [slice(None)] * volume.ndim
            face_slice[i] = -j
            face_slice = tuple(face_slice)
            for neighbor_idx in node_map_array[face_slice].flatten():
                if neighbor_idx != invalid_node_idx:
                    aux_edges[(node_idx, neighbor_idx)] = 1

    if update_progress is not None:
        update_progress(1)

    return node_map, inv_node_map, node_map_array, edges, aux_edges

def edges_to_sparse_matrix(edges, aux_edges, aux_node_idx, n_nodes, ndim):
    n_edges = len(edges)
    n_aux_edges = 0

    for (from_idx, _), _ in aux_edges.items():
        if from_idx == aux_node_idx:
            n_aux_edges += 1

    row = np.empty(shape=(n_edges + n_aux_edges,), dtype=np.int32)
    col = np.empty(shape=(n_edges + n_aux_edges,), dtype=np.int32)
    data = np.empty(shape=(n_edges + n_aux_edges,), dtype=np.float32)

    i = 0
    for (from_idx, to_idx), distance in edges.items():
        row[i] = from_idx
        col[i] = to_idx
        data[i] = distance
        i += 1

    for (from_idx, to_idx), distance in aux_edges.items():
        if from_idx == aux_node_idx:
            row[i] = from_idx
            col[i] = to_idx
            data[i] = distance
            i += 1
    
    assert(i == n_edges + n_aux_edges)

    sparse_edge_matrix = csr_matrix((data, (row, col)), shape=(n_nodes + 2 * ndim, n_nodes + 2 * ndim))

    return sparse_edge_matrix

def distance_matrix_to_volume(inv_node_map, dist_matrix, shape):
    volume = np.empty(shape=shape, dtype=np.float32)
    unreachable_scalar_value = -1
    volume.fill(unreachable_scalar_value)
    for node_idx, node_coord in inv_node_map.items():
        volume[node_coord] = dist_matrix[node_idx]
    volume[volume == np.inf] = unreachable_scalar_value
    volume[volume == np.nan] = unreachable_scalar_value
    return volume

def get_slice_scalars(volume, propagation_direction, slice_number):
    axis_idx = propagation_direction // 2
    start = 0
    increment = 1
    if propagation_direction % 2 == 1:
        increment = -1
        start = volume.shape[axis_idx] - 1

    s = start + increment * slice_number
    assert(s >= 0 and s < volume.shape[axis_idx])

    slice_obj = [slice(None)] * volume.ndim
    slice_obj[axis_idx] = s
    slice_obj = tuple(slice_obj)
    scalars = volume[slice_obj]

    return scalars

def calculate_avg_path_length(volume, propagation_direction):
    unreachable_scalar_value = -1
    axis_idx = propagation_direction // 2
    n_slices = volume.shape[axis_idx]
    column_names = ["Linear Distance", "Actual Distance"]
    table_data = np.empty(shape=(n_slices, 2))
    for i in range(n_slices):
        scalars = get_slice_scalars(volume, propagation_direction, i)
        scalars = np.extract(scalars != unreachable_scalar_value, scalars)
        table_data[i, 0] = i + 1
        table_data[i, 1] = scalars.mean()
    
    return column_names, table_data

def calculate_tortuosity_distribution(volume, propagation_direction):
    unreachable_scalar_value = -1
    axis_idx = propagation_direction // 2
    n_slices = volume.shape[axis_idx]
    last_slice = n_slices - 1

    scalars = get_slice_scalars(volume, propagation_direction, last_slice)
    scalars = np.extract(scalars != unreachable_scalar_value, scalars)
    linear_distance = last_slice + 1
    scalars /= linear_distance

    n_bins = 100
    column_names = ["Tortuosity", "Occurrence"]
    table_data = np.empty(shape=(n_bins, 2))
    bins = np.linspace(1, 6, num=n_bins + 1)
    occurrence, _ = np.histogram(scalars, bins)
    for i in range(n_bins):
        table_data[i, 0] = bins[i]
        table_data[i, 1] = occurrence[i]

    return column_names, table_data

def get_update_progress_fn(progress, stage):
    GRAPH_GENERATION_FRACTION = 0.9
    OTHER_FRACTION = 1 - GRAPH_GENERATION_FRACTION

    if stage == OperatorStages.GraphGeneration:
        def update_progress(value):
            progress.value = round(progress.maximum * value * GRAPH_GENERATION_FRACTION)

        return update_progress

    elif stage == OperatorStages.GraphTraversal:
        def update_progress(value):
            progress.value =  round(progress.maximum * (value * OTHER_FRACTION + GRAPH_GENERATION_FRACTION))

        return update_progress

    else:
        def update_progress(value):
            progress.value = round(progress.maximum * value)

        return update_progress

class TortuosityOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, phase = 1,
                  distance_method = DistanceMethod.Eucledian,
                  propagation_direction = PropagationDirection.Xpos):
        scalars = dataset.active_scalars

        if scalars is None:
            raise RuntimeError("No scalars found!")

        self.progress.maximum = 100
        
        graph_generation_update_progress_fn = get_update_progress_fn(self.progress, OperatorStages.GraphGeneration)
        graph_traversal_update_progress_fn = get_update_progress_fn(self.progress, OperatorStages.GraphTraversal)
        
        self.progress.message = "Converting volume to graph..."
        node_map, inv_node_map, node_map_array, edges, aux_edges = volume_to_graph(scalars, phase, distance_method, graph_generation_update_progress_fn)
        csgraph = edges_to_sparse_matrix(edges, aux_edges, propagation_direction, len(inv_node_map), scalars.ndim)
        graph_traversal_update_progress_fn(0.25)
        self.progress.message = "Propagating along direction..."
        dist_matrix = dijkstra(csgraph=csgraph, directed=False, indices=propagation_direction)
        graph_traversal_update_progress_fn(0.75)
        self.progress.message = "Generating outputs..."
        result = distance_matrix_to_volume(inv_node_map, dist_matrix, scalars.shape)
        graph_traversal_update_progress_fn(1)

        dataset.active_scalars = result

        # Create a spreadsheet data set from table data
        returnValues = {}

        column_names, table_data = calculate_avg_path_length(result, propagation_direction)
        returnValues["path_length"] = tomviz.utils.make_spreadsheet(column_names, table_data)

        column_names, table_data = calculate_tortuosity_distribution(result, propagation_direction)
        returnValues["tortuosity_distribution"] = tomviz.utils.make_spreadsheet(column_names, table_data)

        return returnValues

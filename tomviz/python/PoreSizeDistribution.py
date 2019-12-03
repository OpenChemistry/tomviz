import tomviz.operators
import tomviz.utils

import os
import numpy as np
import scipy.ndimage

PORE_PHASE = 1
MATTER_PHASE = 0


def coord_iterator(extent):
    if len(extent) == 1:
        for i in range(extent[0][0], extent[0][1]):
            yield (i, )
    elif len(extent) > 1:
        for i in range(extent[0][0], extent[0][1]):
            for c in coord_iterator(extent[1:]):
                yield (i, ) + c


def make_sphere_structure(radius, ndim):
    shape = (2 * radius + 1,) * ndim
    center = (radius,) * ndim
    rr = radius * radius
    structure = np.empty(shape=shape, dtype=np.uint8)
    extent = tuple((0, s) for s in shape)
    for coord in coord_iterator(extent):
        if sum((a - b) * (a - b) for a, b in zip(coord, center)) <= rr:
            structure[coord] = PORE_PHASE
        else:
            structure[coord] = MATTER_PHASE
    return structure


def segment_pore(volume, threshold):
    segmented = np.empty(shape=volume.shape, dtype=np.uint8)
    segmented.fill(PORE_PHASE)
    segmented[volume > threshold] = MATTER_PHASE
    return segmented


def make_distance_map(volume):
    shape = tuple(s + 2 for s in volume.shape)
    distance = np.empty(shape=shape, dtype=volume.dtype)
    distance.fill(MATTER_PHASE)
    volume_span = tuple(slice(1, s + 1) for s in volume.shape)
    distance[volume_span] = volume
    distance = scipy.ndimage.distance_transform_edt(distance)
    return distance[volume_span]


def get_dilation_slices(coord, shape, structure_shape):
    volume_slice = []
    structure_slice = []
    for c, r, s in zip(coord, shape, structure_shape):
        start = c - s // 2
        stop = start + s
        start_offset = 0
        stop_offset = 0
        if (start < 0):
            start_offset = -start
        if (stop > r):
            stop_offset = r - stop

        volume_slice.append(slice(start + start_offset, stop + stop_offset))
        structure_slice.append(slice(start_offset, s + stop_offset))

    return tuple(volume_slice), tuple(structure_slice)


def binary_dilation(a, structure):
    """
    Alternative implementation of scipy.ndimage.binary_dilation

    The scipy version becomes very with larger structuring elements,
    and it also throws memory errors.

    This version is slower for smaller structuring elements, but becomes
    faster with larger ones, and it also appears to be more stable
    """
    dil = np.zeros(shape=a.shape, dtype=np.bool)
    extent = tuple((0, s) for s in a.shape)
    for coord in coord_iterator(extent):
        if (a[coord]):
            a_slice, structure_slice = get_dilation_slices(coord, a.shape,
                                                           structure.shape)
            dil[a_slice] = structure[structure_slice] | dil[a_slice]
    return dil


def get_dilation_count(volume, distance_map, radius, dilation_fn=None):
    if dilation_fn is None:
        dilation_fn = scipy.ndimage.binary_dilation

    structure = make_sphere_structure(radius, volume.ndim)
    pore_radii = distance_map >= radius
    r_dil = dilation_fn(pore_radii, structure=structure)
    r_dil = r_dil & volume
    return r_dil, np.sum(r_dil)


class PoreSizeDistribution(tomviz.operators.CancelableOperator):

    def transform(self, dataset, threshold=127, radius_spacing=1,
                  save_to_file=False, output_folder=""):
        scalars = dataset.active_scalars

        if scalars is None:
            raise RuntimeError("No scalars found!")

        if save_to_file and not os.access(output_folder, os.W_OK):
            import warnings
            save_to_file = False
            warnings.warn(
                "Unable to write to destination folder %s" % output_folder)

        self.progress.maximum = 100
        self.progress.value = 0
        self.progress.message = "Generating the pore size map..."

        total_volume = scalars.size
        cavity = segment_pore(scalars, threshold)
        distance_map = make_distance_map(cavity)

        dataset.active_scalars = distance_map

        max_r = int(np.max(distance_map))
        # Ensure max_r is always included in the radius list
        pore_radius = list(range(1, max_r, radius_spacing))
        if pore_radius[-1] != max_r:
            pore_radius.append(max_r)

        pore_volume = []

        n = len(pore_radius)

        self.progress.message = "Calculating pore size distribution..."

        for i, r in enumerate(pore_radius):
            self.progress.value = int(100 * (i + 1) / (n + 1))
            _, count = get_dilation_count(cavity, distance_map, r,
                                          binary_dilation)
            pore_volume.append(count / total_volume)

        self.progress.value = 100

        return_values = {}

        column_names = ["Pore radius", "Pore volume"]
        table_data = np.empty(shape=(n, 2))

        table_data[:, 0] = pore_radius
        table_data[:, 1] = pore_volume

        if save_to_file:
            filename = "pore_size_distribution.csv"
            np.savetxt(os.path.join(output_folder, filename), table_data,
                       delimiter=", ", header=", ".join(column_names))

        table = tomviz.utils.make_spreadsheet(column_names, table_data)
        return_values["pore_size_distribution"] = table

        return return_values

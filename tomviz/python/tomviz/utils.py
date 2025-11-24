import copy
import functools
import math

import numpy as np

from tomviz._internal import in_application
from tomviz._internal import require_internal_mode
from tomviz.internal_utils import _minmax
if in_application():
    from vtk import vtkTable
    from tomviz.internal_dataset import Dataset
else:
    from tomviz.external_dataset import Dataset


def zoom_shape(input: np.ndarray, zoom: np.ndarray) -> tuple[int]:
    """
    Returns the shape of the output array for scipy.ndimage.interpolation.zoom
    """

    if isinstance(zoom, (int, float,)):
        zoom = [zoom] * input.ndim

    return tuple(
        [int(round(i * j)) for i, j in zip(input.shape, zoom)])


def rotate_shape(input: np.ndarray, angle: float,
                 axes: tuple[int, int]) -> tuple[int]:
    """
    Returns the shape of the output array of scipy.ndimage.interpolation.rotate
    derived from: https://github.com/scipy/scipy/blob/v0.16.1/scipy/ndimage/ \
    interpolation.py #L578. We are duplicating the code here so we can generate
    an array of the right shape and array order to pass into the rotate
    function.
    """

    axes = list(axes)
    rank = input.ndim
    if axes[0] < 0:
        axes[0] += rank
    if axes[1] < 0:
        axes[1] += rank
    if axes[0] < 0 or axes[1] < 0 or axes[0] > rank or axes[1] > rank:
        raise RuntimeError('invalid rotation plane specified')
    if axes[0] > axes[1]:
        axes = axes[1], axes[0]
    angle = np.pi / 180 * angle
    m11 = math.cos(angle)
    m12 = math.sin(angle)
    m21 = -math.sin(angle)
    m22 = math.cos(angle)
    matrix = np.array([[m11, m12],
                       [m21, m22]], dtype=np.float64)
    iy = input.shape[axes[0]]
    ix = input.shape[axes[1]]
    mtrx = np.array([[m11, -m21],
                     [-m12, m22]], dtype=np.float64)
    minc = [0, 0]
    maxc = [0, 0]
    coor = np.dot(mtrx, [0, ix])
    minc, maxc = _minmax(coor, minc, maxc)
    coor = np.dot(mtrx, [iy, 0])
    minc, maxc = _minmax(coor, minc, maxc)
    coor = np.dot(mtrx, [iy, ix])
    minc, maxc = _minmax(coor, minc, maxc)
    oy = int(maxc[0] - minc[0] + 0.5)
    ox = int(maxc[1] - minc[1] + 0.5)
    offset = np.zeros((2,), dtype=np.float64)
    offset[0] = float(oy) / 2.0 - 0.5
    offset[1] = float(ox) / 2.0 - 0.5
    offset = np.dot(matrix, offset)
    tmp = np.zeros((2,), dtype=np.float64)
    tmp[0] = float(iy) / 2.0 - 0.5
    tmp[1] = float(ix) / 2.0 - 0.5
    offset = tmp - offset
    output_shape = list(input.shape)
    output_shape[axes[0]] = oy
    output_shape[axes[1]] = ox

    return output_shape


def apply_to_each_array(func):
    """
    This decorator causes an operator `transform()` function to
    automatically run one time for every array.

    For example, for the rotation operator:

    .. code-block:: python

        @apply_to_each_array
        def transform(dataset, rotation_angle=90.0, rotation_axis=0):
            # ...

    The `transform()` function will be executed one time for every
    array. When executing the `transform()` function, the `dataset`
    object will only contain a single array on `dataset.active_scalars`
    each time.

    This allows an operator `transform()` function to be written in
    a way that appears to only operate on one array, but then automatically
    be ran multiple times to apply to each array.

    The final `dataset` object will automatically contain each of the
    transformed arrays on it.
    """
    is_method = (
        func.__name__ != func.__qualname__ and
        '.<locals>.' not in func.__qualname__
    )
    dataset_idx = 1 if is_method else 0

    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        dataset = args[dataset_idx]
        if dataset.num_scalars == 1:
            # Just run the function like we normally would...
            return func(*args, **kwargs)

        num_arrays = dataset.num_scalars
        array_names = dataset.scalars_names
        active_name = dataset.active_name

        is_internal = in_application()

        if is_internal:
            # Run the function multiple times. Each time with a single,
            # different array on the shallow-copied data object
            from vtk import vtkImageData
            orig_do = dataset._data_object
            pd = orig_do.GetPointData()
            all_arrays = [pd.GetAbstractArray(i) for i in range(num_arrays)]

            # Remove all arrays
            while pd.GetNumberOfArrays() > 0:
                pd.RemoveArray(0)
        else:
            all_arrays = [dataset.arrays[name] for name in array_names]
            dataset.arrays.clear()
            orig_dataset = dataset

        output_arrays = []
        results = []
        for i, name in enumerate(array_names):
            if is_internal:
                if i == num_arrays - 1:
                    # Use the original data object
                    image_data = orig_do
                else:
                    image_data = vtkImageData()
                    image_data.ShallowCopy(orig_do)

                this_pd = image_data.GetPointData()
                this_pd.AddArray(all_arrays[i])
                this_pd.SetActiveScalars(name)
                dataset._data_object = image_data
            else:
                if i == num_arrays - 1:
                    # Use the original dataset for the final one
                    dataset = orig_dataset
                else:
                    dataset = copy.deepcopy(orig_dataset)

                dataset.arrays[name] = all_arrays[i]
                dataset.active_name = name

            # Put the dataset where it belongs in the argument list
            new_args = (
                list(args[:dataset_idx]) +
                [dataset] +
                list(args[dataset_idx + 1:])
            )
            print('Transforming array:', name)
            result = func(*new_args, **kwargs)
            results.append(result)

            if is_internal:
                output_arrays.append(this_pd.GetAbstractArray(0))
            else:
                output_arrays.append(dataset.arrays[name])

        if is_internal:
            # Now add back in the arrays in the same order
            this_pd.RemoveArray(0)
            for array in output_arrays:
                this_pd.AddArray(array)

            # Set the active one
            this_pd.SetActiveScalars(active_name)
        else:
            # The metadata should have been modified on this dataset
            # object from the last call to the function
            dataset.arrays.clear()
            for name, array in zip(array_names, output_arrays):
                dataset.arrays[name] = array

            dataset.active_name = active_name

        # For any data sources in the result, add all the scalars to it
        if isinstance(result, dict):
            for k, v in result.items():
                if isinstance(v, Dataset):
                    # Rename the active array
                    v.rename_active(array_names[-1])
                    # Go back through the other results and set scalars on this
                    # one
                    for i, other_result in enumerate(results[:-1]):
                        if (
                            isinstance(other_result, dict) and
                            isinstance(other_result.get(k), Dataset)
                        ):
                            other_dataset = other_result[k]
                            other_dataset.rename_active(array_names[i])
                            v.set_scalars(other_dataset.active_name,
                                          other_dataset.active_scalars)

        # Return the final result
        return result

    return wrapper


def pad_array(array: np.ndarray, padding: int, tilt_axis: int) -> np.ndarray:
    """Add padding to an array. Ignore the tilt axis.

    The resulting padded array can eventually be depadded by calling the
    `depad_array()` function.
    """
    if padding <= 0:
        return array

    pad_list = []
    for i in range(3):
        pad_list.append([0, 0] if i == tilt_axis else [padding, padding])

    return np.pad(array, pad_list)


def depad_array(array: np.ndarray, padding: int, tilt_axis: int) -> np.ndarray:
    """Remove padding from an array. Ignore the tilt axis."""
    if padding <= 0:
        return array

    slice_list = []
    for i in range(3):
        start = padding if i != tilt_axis else 0
        end = padding * -1 if i != tilt_axis else array.shape[i]
        slice_list.append(slice(start, end))
    return array[tuple(slice_list)]


def make_spreadsheet(column_names: list[str], table: np.ndarray) -> 'vtkTable':
    """Make a spreadsheet object to use within Tomviz

    If returned from an operator, this will ultimately appear within the
    pipeline, and will be save-able to a JSON file.

    The output of this function ought to be included in the returned
    dictionary. For example:

    .. code-block:: python

        spreadsheet = utils.make_spreadsheet(column_names, table)

        return {
            'table_data': spreadsheet,
        }
    """
    # column_names is a list of strings
    # table is a 2D numpy.ndarray
    # returns a vtkTable object that stores the table content
    require_internal_mode()

    # Create a vtkTable to store the output.
    rows = table.shape[0]

    if (table.shape[1] != len(column_names)):
        print('Warning: table number of columns differs from number of '
              'column names')
        return

    from vtk import vtkTable, vtkFloatArray
    vtk_table = vtkTable()
    for (column, name) in enumerate(column_names):
        array = vtkFloatArray()
        array.SetName(name)
        array.SetNumberOfComponents(1)
        array.SetNumberOfTuples(rows)
        vtk_table.AddColumn(array)

        for row in range(0, rows):
            array.InsertValue(row, table[row, column])

    return vtk_table

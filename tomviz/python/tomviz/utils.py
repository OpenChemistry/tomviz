# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
import copy
import functools
import math
import numpy as np
from tomviz._internal import in_application
from tomviz._internal import require_internal_mode
from tomviz._internal import with_vtk_dataobject
from tomviz._internal import with_dataset
from tomviz.internal_dataset import Dataset as InternalDataset
# Only import vtk if we are running within the tomviz application ( not cli )
if in_application():
    import vtk.numpy_interface.dataset_adapter as dsa
    import vtk.util.numpy_support as np_s


@with_vtk_dataobject
def get_scalars(dataobject, name=None):
    do = dsa.WrapDataObject(dataobject)
    if name is not None:
        rawarray = do.PointData.GetAbstractArray(name)
    else:
        rawarray = do.PointData.GetScalars()
    vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    return vtkarray


@with_vtk_dataobject
def get_active_scalars_name(dataobject):
    return dataobject.GetPointData().GetScalars().GetName()


def is_numpy_vtk_type(newscalars):
    # Indicate whether the type is known/supported by VTK to NumPy routines.
    require_internal_mode()

    try:
        np_s.get_vtk_array_type(newscalars.dtype)
    except TypeError:
        return False
    else:
        return True


@with_vtk_dataobject
def set_scalars(dataobject, newscalars):
    do = dsa.WrapDataObject(dataobject)
    oldscalars = do.PointData.GetScalars()
    if oldscalars is None:
        name = "scalars"
    else:
        name = oldscalars.GetName()
        del oldscalars

    if not is_numpy_vtk_type(newscalars):
        newscalars = newscalars.astype(np.float32)

    do.PointData.append(newscalars, name)
    do.PointData.SetActiveScalars(name)


@with_vtk_dataobject
def get_array(dataobject, name=None, order='F'):
    scalars_array = get_scalars(dataobject, name=name)
    if order == 'F':
        scalars_array3d = np.reshape(scalars_array,
                                     (dataobject.GetDimensions()),
                                     order=order)
    else:
        scalars_array3d = np.reshape(scalars_array,
                                     (dataobject.GetDimensions()[::-1]),
                                     order=order)
    return scalars_array3d


@with_vtk_dataobject
def array_names(dataobject):
    do = dsa.WrapDataObject(dataobject)
    num_arrays = do.PointData.GetNumberOfArrays()
    return [do.PointData.GetArrayName(i) for i in range(num_arrays)]


@with_vtk_dataobject
def arrays(dataobject):
    """
    Iterate over (name, array) for the arrays in this dataset.

    :param dataobject The incoming dataset
    :type: vtkDataObject
    """
    for name in array_names(dataobject):
        yield (name, get_array(dataobject, name))


@with_vtk_dataobject
def set_array(dataobject, newarray, minextent=None, isFortran=True, name=None):
    # Set the extent if needed, i.e. if the minextent is not the same as
    # the data object starting index, or if the newarray shape is not the same
    # as the size of the dataobject.
    # isFortran indicates whether the NumPy array has Fortran-order indexing,
    # i.e. i,j,k indexing. If isFortran is False, then the NumPy array uses
    # C-order indexing, i.e. k,j,i indexing.
    if not isFortran:
        # Flatten according to array.flags
        arr = newarray.ravel(order='A')
        if newarray.flags.f_contiguous:
            vtkshape = newarray.shape
        else:
            vtkshape = newarray.shape[::-1]
    elif np.isfortran(newarray):
        arr = newarray.reshape(-1, order='F')
        vtkshape = newarray.shape
    else:
        # This used to print a warning, but we shouldn't worry about
        # it...
        vtkshape = newarray.shape
        tmp = np.asfortranarray(newarray)
        arr = tmp.reshape(-1, order='F')

    if not is_numpy_vtk_type(arr):
        arr = arr.astype(np.float32)

    if minextent is None:
        minextent = dataobject.GetExtent()[::2]
    sameindex = list(minextent) == list(dataobject.GetExtent()[::2])
    sameshape = list(vtkshape) == list(dataobject.GetDimensions())
    if not sameindex or not sameshape:
        extent = 6*[0]
        extent[::2] = minextent
        extent[1::2] = \
            [x + y - 1 for (x, y) in zip(minextent, vtkshape)]
        dataobject.SetExtent(extent)

    # Now replace the scalars array with the new array.
    vtkarray = np_s.numpy_to_vtk(arr)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    do = dsa.WrapDataObject(dataobject)

    if name is None:
        oldscalars = do.PointData.GetScalars()
        arrayname = "Scalars"
        if oldscalars is not None:
            arrayname = oldscalars.GetName()
    else:
        arrayname = name

    do.PointData.append(arr, arrayname)

    if do.PointData.GetNumberOfArrays() == 1:
        do.PointData.SetActiveScalars(arrayname)


@with_vtk_dataobject
def get_tilt_angles(dataobject):
    # Get the tilt angles array
    do = dsa.WrapDataObject(dataobject)
    rawarray = do.FieldData.GetArray('tilt_angles')
    if isinstance(rawarray, dsa.VTKNoneArray):
        return None
    vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
    vtkarray.Association = dsa.ArrayAssociation.FIELD
    return vtkarray


@with_vtk_dataobject
def set_tilt_angles(dataobject, newarray):
    # replace the tilt angles with the new array
    from vtkmodules.util.vtkConstants import VTK_DOUBLE
    # deep copy avoids having to keep numpy array around, but is more
    # expensive.  I don't expect tilt_angles to be a big array though.
    vtkarray = np_s.numpy_to_vtk(newarray, deep=1, array_type=VTK_DOUBLE)
    vtkarray.Association = dsa.ArrayAssociation.FIELD
    vtkarray.SetName('tilt_angles')
    do = dsa.WrapDataObject(dataobject)
    do.FieldData.RemoveArray('tilt_angles')
    do.FieldData.AddArray(vtkarray)


@with_vtk_dataobject
def get_coordinate_arrays(dataobject):
    """Returns a triple of Numpy arrays containing x, y, and z coordinates for
    each point in the dataset. This can be used to evaluate a function at each
    point, for instance.
    """
    assert dataobject.IsA("vtkImageData"), "Dataset must be a vtkImageData"

    # Create meshgrid for image
    spacing = dataobject.GetSpacing()
    origin = dataobject.GetOrigin()
    dims = dataobject.GetDimensions()
    x = [origin[0] + (spacing[0] * i) for i in range(dims[0])]
    y = [origin[1] + (spacing[1] * i) for i in range(dims[1])]
    z = [origin[2] + (spacing[2] * i) for i in range(dims[2])]

    # The funny ordering is to match VTK's convention for point storage
    yy, xx, zz = np.meshgrid(y, x, z)

    return (xx, yy, zz)


@with_vtk_dataobject
def make_child_dataset(dataobject):
    """Creates a child dataset with the same size as the reference_dataset.
    """
    from vtk import vtkImageData
    new_child = vtkImageData()
    new_child.CopyStructure(dataobject)
    input_spacing = dataobject.GetSpacing()
    # For a reconstruction we copy the X spacing from the input dataset
    child_spacing = (input_spacing[0], input_spacing[1], input_spacing[0])
    new_child.SetSpacing(child_spacing)

    return new_child


@with_dataset
def connected_components(dataset, background_value=0, progress_callback=None):
    try:
        import numpy as np
        import itk
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    if np.issubdtype(dataset.active_scalars.dtype, np.floating):
        raise Exception(
            "Connected Components works only on images with integral types.")

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image. The input is assumed to have an integral type.
        # Take care of casting to an unsigned short image so we can store up
        # to 65,535 connected components (the number of connected components
        # is limited to the maximum representable number in the voxel type
        # of the input image in the ConnectedComponentsFilter).
        array = dataset.active_scalars.astype(np.uint16)
        itk_image = itk.GetImageViewFromArray(array)
        itk_image.SetSpacing(dataset.spacing)
        itk_image_type = type(itk_image)

        # ConnectedComponentImageFilter
        connected_filter = itk.ConnectedComponentImageFilter[
            itk_image_type, itk_image_type].New()
        connected_filter.SetBackgroundValue(background_value)
        connected_filter.SetInput(itk_image)

        if progress_callback is not None:

            def connected_progress_func():
                progress = connected_filter.GetProgress()
                abort = progress_callback(progress * 0.5)
                connected_filter.SetAbortGenerateData(abort)

            connected_observer = itk.PyCommand.New()
            connected_observer.SetCommandCallable(connected_progress_func)
            connected_filter.AddObserver(itk.ProgressEvent(),
                                         connected_observer)

        # Relabel filter. This will compress the label numbers to a
        # continugous range between 1 and n where n is the number of
        # labels. It will also sort the components from largest to
        # smallest, where the largest component has label 1, the
        # second largest has label 2, and so on...
        relabel_filter = itk.RelabelComponentImageFilter[
            itk_image_type, itk_image_type].New()
        relabel_filter.SetInput(connected_filter.GetOutput())
        relabel_filter.SortByObjectSizeOn()

        if progress_callback is not None:

            def relabel_progress_func():
                progress = relabel_filter.GetProgress()
                abort = progress_callback(progress * 0.5 + 0.5)
                relabel_filter.SetAbortGenerateData(abort)

            relabel_observer = itk.PyCommand.New()
            relabel_observer.SetCommandCallable(relabel_progress_func)
            relabel_filter.AddObserver(itk.ProgressEvent(), relabel_observer)

        try:
            relabel_filter.Update()
        except RuntimeError:
            return

        itk_image_data = relabel_filter.GetOutput()
        label_buffer = itk.PyBuffer[
            itk_image_type].GetArrayFromImage(itk_image_data)

        # Flip the labels so that the largest component has the highest label
        # value, e.g., the labeling ordering by size goes from [1, 2, ... N] to
        # [N, N-1, N-2, ..., 1]. Note that zero is the background value, so we
        # do not want to change it.
        import numpy as np
        minimum = 1  # Minimum label is always 1, background is 0
        maximum = np.max(label_buffer)

        # Try more memory-efficient approach
        gt_zero = label_buffer > 0
        label_buffer[gt_zero] = minimum - label_buffer[gt_zero] + maximum

        # Transpose the data to Fortran indexing
        dataset.active_scalars = label_buffer.transpose([2, 1, 0])

    except Exception as exc:
        print("Problem encountered while running ConnectedComponents")
        raise exc


@with_dataset
def label_object_principal_axes(dataset, label_value):
    import numpy as np
    from tomviz import utils

    labels = dataset.active_scalars
    num_voxels = np.sum(labels == label_value)
    xx, yy, zz = utils.get_coordinate_arrays(dataset)

    data = np.zeros((num_voxels, 3))
    selection = labels == label_value
    assert np.any(selection), \
        "No voxels with label %d in label map" % label_value
    data[:, 0] = xx[selection]
    data[:, 1] = yy[selection]
    data[:, 2] = zz[selection]

    # Compute PCA on coordinates
    from scipy import linalg as la
    m, n = data.shape
    center = data.mean(axis=0)
    data -= center
    R = np.cov(data, rowvar=False)
    evals, evecs = la.eigh(R)
    idx = np.argsort(evals)[::-1]
    evecs = evecs[:, idx]
    evals = evals[idx]
    return (evecs, center)


def make_dataset(x, y, z, dataobject, generate_data_function, **kwargs):
    from vtk import VTK_DOUBLE
    array = np.zeros((x, y, z), order='F')
    generate_data_function(array, **kwargs)
    dataobject.SetOrigin(0, 0, 0)
    dataobject.SetSpacing(1, 1, 1)
    dataobject.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
    flat_array = array.reshape(-1, order='F')
    vtkarray = np_s.numpy_to_vtk(flat_array, deep=1, array_type=VTK_DOUBLE)
    vtkarray.SetName("generated_scalars")
    dataobject.GetPointData().SetScalars(vtkarray)


@with_vtk_dataobject
def mark_as_volume(dataobject):
    from vtk import vtkTypeInt8Array
    fd = dataobject.GetFieldData()
    arr = fd.GetArray("tomviz_data_source_type")
    if arr is None:
        arr = vtkTypeInt8Array()
        arr.SetNumberOfComponents(1)
        arr.SetNumberOfTuples(1)
        arr.SetName("tomviz_data_source_type")
        fd.AddArray(arr)
    arr.SetTuple1(0, 0)


@with_vtk_dataobject
def mark_as_tiltseries(dataobject):
    from vtk import vtkTypeInt8Array
    fd = dataobject.GetFieldData()
    arr = fd.GetArray("tomviz_data_source_type")
    if arr is None:
        arr = vtkTypeInt8Array()
        arr.SetNumberOfComponents(1)
        arr.SetNumberOfTuples(1)
        arr.SetName("tomviz_data_source_type")
        fd.AddArray(arr)
    arr.SetTuple1(0, 1)


@with_vtk_dataobject
def set_size(dataobject, x=None, y=None, z=None):
    axes = []
    lengths = []
    if x is not None:
        axes.append(0)
        lengths.append(x)
    if y is not None:
        axes.append(1)
        lengths.append(y)
    if z is not None:
        axes.append(2)
        lengths.append(z)

    extent = dataobject.GetExtent()
    spacing = list(dataobject.GetSpacing())
    for axis, new_length in zip(axes, lengths):
        spacing[axis] = \
            new_length / (extent[2 * axis + 1] - extent[2 * axis] + 1)

    dataobject.SetSpacing(spacing)


@with_vtk_dataobject
def get_spacing(dataobject):
    return dataobject.GetSpacing()


@with_vtk_dataobject
def set_spacing(dataobject, x=None, y=None, z=None):
    spacing = list(dataobject.GetSpacing())
    if x is not None:
        spacing[0] = x
    if y is not None:
        spacing[1] = y
    if z is not None:
        spacing[2] = z

    dataobject.SetSpacing(spacing)


def make_spreadsheet(column_names, table):
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


def zoom_shape(input, zoom):
    """
    Returns the shape of the output array for scipy.ndimage.interpolation.zoom
    :param input The input array
    :type input: ndarray
    :param zoom The zoom factor
    :type zoom: ndarray
    """

    if isinstance(zoom, (int, float,)):
        zoom = [zoom] * input.ndim

    return tuple(
        [int(round(i * j)) for i, j in zip(input.shape, zoom)])


def _minmax(coor, minc, maxc):
    if coor[0] < minc[0]:
        minc[0] = coor[0]
    if coor[0] > maxc[0]:
        maxc[0] = coor[0]
    if coor[1] < minc[1]:
        minc[1] = coor[1]
    if coor[1] > maxc[1]:
        maxc[1] = coor[1]

    return minc, maxc


def rotate_shape(input, angle, axes):
    """
    Returns the shape of the output array of scipy.ndimage.interpolation.rotate
    derived from: https://github.com/scipy/scipy/blob/v0.16.1/scipy/ndimage/ \
    interpolation.py #L578. We are duplicating the code here so we can generate
    an array of the right shape and array order to pass into the rotate
    function.

    :param input The input array
    :type: ndarray
    :param angle The rotation angle in degrees.
    :type: float
    :param axes The two axes that define the plane of rotation.
                Default is the first two axes.
    :type: tuple of 2 ints
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


@with_vtk_dataobject
def set_principal_axes(dataobject, axes):
    from vtkmodules.vtkCommonCore import vtkFloatArray

    fd = dataobject.GetFieldData()

    axis_array = vtkFloatArray()
    axis_array.SetName('PrincipalAxes')
    axis_array.SetNumberOfComponents(3)
    axis_array.SetNumberOfTuples(3)
    axis_array.InsertTypedTuple(0, list(axes[:, 0]))
    axis_array.InsertTypedTuple(1, list(axes[:, 1]))
    axis_array.InsertTypedTuple(2, list(axes[:, 2]))
    fd.RemoveArray('PrincipalAxis')
    fd.AddArray(axis_array)


@with_vtk_dataobject
def get_principal_axes(dataobject, principal_axis):
    fd = dataobject.GetFieldData()
    axis_array = fd.GetArray('PrincipalAxes')
    assert axis_array is not None, \
        "Dataset does not have a PrincipalAxes field data array"
    assert axis_array.GetNumberOfTuples() == 3, \
        "PrincipalAxes array requires 3 tuples"
    assert axis_array.GetNumberOfComponents() == 3, \
        "PrincipalAxes array requires 3 components"
    assert principal_axis >= 0 and principal_axis <= 2, \
        "Invalid principal axis. Must be in range [0, 2]."

    return np.array(axis_array.GetTuple(principal_axis))


@with_vtk_dataobject
def set_center(dataobject, center):
    from vtkmodules.vtkCommonCore import vtkFloatArray

    fd = dataobject.GetFieldData()

    center_array = vtkFloatArray()
    center_array.SetName('Center')
    center_array.SetNumberOfComponents(3)
    center_array.SetNumberOfTuples(1)
    center_array.InsertTypedTuple(0, list(center))
    fd.RemoveArray('Center')
    fd.AddArray(center_array)


@with_vtk_dataobject
def get_center(dataobject):
    fd = dataobject.GetFieldData()
    center_array = fd.GetArray('Center')
    assert center_array is not None, \
        "Dataset does not have a Center field data array"
    assert center_array.GetNumberOfTuples() == 1, \
        "Center array requires 1 tuple"
    assert center_array.GetNumberOfComponents() == 3, \
        "Center array requires 3 components"

    return np.array(center_array.GetTuple(0))


def apply_to_each_array(func):

    @functools.wraps(func)
    def wrapper(dataset, *args, **kwargs):
        if dataset.num_scalars == 1:
            # Just run the function like we normally would...
            return func(dataset, *args, **kwargs)

        num_arrays = dataset.num_scalars
        array_names = dataset.scalars_names
        active_name = dataset.active_name

        is_internal = isinstance(dataset, InternalDataset)

        if is_internal:
            # Run the function multiple times. Each time with a single, different
            # array on the shallow-copied data object
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

            print('Transforming array:', name)
            result = func(dataset, *args, **kwargs)
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

        # Return the final result
        return result

    return wrapper


def pad_array(array, padding, tilt_axis):
    # Add padding to an array. Ignore the tilt axis.
    if padding <= 0:
        return array

    pad_list = []
    for i in range(3):
        pad_list.append([0, 0] if i == tilt_axis else [padding, padding])

    return np.pad(array, pad_list)


def depad_array(array, padding, tilt_axis):
    # Remove padding from an array. Ignore the tilt axis.
    if padding <= 0:
        return array

    slice_list = []
    for i in range(3):
        start = padding if i != tilt_axis else 0
        end = padding * -1 if i != tilt_axis else array.shape[i]
        slice_list.append(slice(start, end))
    return array[tuple(slice_list)]

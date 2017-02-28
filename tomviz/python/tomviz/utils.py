# -*- coding: utf-8 -*-

###############################################################################
#
#  This source file is part of the tomviz project.
#
#  Copyright Kitware, Inc.
#
#  This source code is released under the New BSD License, (the "License").
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###############################################################################

import math
import numpy as np
import vtk.numpy_interface.dataset_adapter as dsa
import vtk.util.numpy_support as np_s


def get_scalars(dataobject):
    do = dsa.WrapDataObject(dataobject)
    # get the first
    rawarray = do.PointData.GetScalars()
    vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    return vtkarray


def set_scalars(dataobject, newscalars):
    do = dsa.WrapDataObject(dataobject)
    oldscalars = do.PointData.GetScalars()
    name = oldscalars.GetName()
    del oldscalars

    # handle the case if the newscalars array has a type that
    # cannot be passed on to VTK. In which case, we convert to
    # convert to float64
    vtk_typecode = np_s.get_vtk_array_type(newscalars.dtype)
    if vtk_typecode is None:
        newscalars = newscalars.astype(np.float64)
    do.PointData.append(newscalars, name)
    do.PointData.SetActiveScalars(name)


def get_array(dataobject, order='F'):
    scalars_array = get_scalars(dataobject)
    if order == 'F':
        scalars_array3d = np.reshape(scalars_array,
                                     (dataobject.GetDimensions()),
                                     order=order)
    else:
        scalars_array3d = np.reshape(scalars_array,
                                     (dataobject.GetDimensions()[::-1]),
                                     order=order)
    return scalars_array3d


def set_array(dataobject, newarray, minextent=None, isFortran=True):
    # Set the extent if needed, i.e. if the minextent is not the same as
    # the data object starting index, or if the newarray shape is not the same
    # as the size of the dataobject.
    # isFortran indicates whether the NumPy array has Fortran-order indexing,
    # i.e. i,j,k indexing. If isFortran is False, then the NumPy array uses
    # C-order indexing, i.e. k,j,i indexing.

    if isFortran is False:
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
        print('Warning, array does not have Fortran order, making deep copy '
              'and fixing...')
        vtkshape = newarray.shape
        tmp = np.asfortranarray(newarray)
        arr = tmp.reshape(-1, order='F')
        print('...done.')

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
    oldscalars = do.PointData.GetScalars()
    arrayname = "Scalars"
    if oldscalars is not None:
        arrayname = oldscalars.GetName()
    del oldscalars
    do.PointData.append(arr, arrayname)
    do.PointData.SetActiveScalars(arrayname)


def get_tilt_angles(dataobject):
    # Get the tilt angles array
    do = dsa.WrapDataObject(dataobject)
    rawarray = do.FieldData.GetArray('tilt_angles')
    vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
    vtkarray.Association = dsa.ArrayAssociation.FIELD
    return vtkarray


def set_tilt_angles(dataobject, newarray):
    # replace the tilt angles with the new array
    from vtk import VTK_DOUBLE
    # deep copy avoids having to keep numpy array around, but is more
    # expensive.  I don't expect tilt_angles to be a big array though.
    vtkarray = np_s.numpy_to_vtk(newarray, deep=1, array_type=VTK_DOUBLE)
    vtkarray.Association = dsa.ArrayAssociation.FIELD
    vtkarray.SetName('tilt_angles')
    do = dsa.WrapDataObject(dataobject)
    do.FieldData.RemoveArray('tilt_angles')
    do.FieldData.AddArray(vtkarray)


def get_coordinate_arrays(dataset):
    """Returns a triple of Numpy arrays containing x, y, and z coordinates for
    each point in the dataset. This can be used to evaluate a function at each
    point, for instance.
    """
    assert dataset.IsA("vtkImageData"), "Dataset must be a vtkImageData"

    # Create meshgrid for image
    spacing = dataset.GetSpacing()
    origin = dataset.GetOrigin()
    dims = dataset.GetDimensions()
    x = [origin[0] + (spacing[0] * i) for i in range(dims[0])]
    y = [origin[1] + (spacing[1] * i) for i in range(dims[1])]
    z = [origin[2] + (spacing[2] * i) for i in range(dims[2])]

    # The funny ordering is to match VTK's convention for point storage
    yy, xx, zz = np.meshgrid(y, x, z)

    return (xx, yy, zz)


def connected_components(dataset, background_value=0, progress_callback=None):
    try:
        import itk
        import itkTypes
        import vtk
        from tomviz import itkutils
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    scalarType = dataset.GetScalarType()
    if scalarType == vtk.VTK_FLOAT or scalarType == vtk.VTK_DOUBLE:
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
        itk_image = itkutils.convert_vtk_to_itk_image(dataset, itkTypes.US)
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
        minimum = 1 # Minimum label is always 1, background is 0
        maximum = np.max(label_buffer)

        # Try more memory-efficient approach
        gt_zero = label_buffer > 0
        label_buffer[gt_zero] = minimum - label_buffer[gt_zero] + maximum

        set_array(dataset, label_buffer, isFortran=False)
    except Exception as exc:
        print("Problem encountered while running ConnectedComponents")
        raise exc


def label_object_principal_axes(dataset, label_value):
    import numpy as np
    from tomviz import utils
    labels = utils.get_array(dataset)
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


def make_dataset(x, y, z, dataset, generate_data_function, **kwargs):
    from vtk import VTK_DOUBLE
    array = np.zeros((x, y, z), order='F')
    generate_data_function(array, **kwargs)
    dataset.SetOrigin(0, 0, 0)
    dataset.SetSpacing(1, 1, 1)
    dataset.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
    flat_array = array.reshape(-1, order='F')
    vtkarray = np_s.numpy_to_vtk(flat_array, deep=1, array_type=VTK_DOUBLE)
    vtkarray.SetName("generated_scalars")
    dataset.GetPointData().SetScalars(vtkarray)


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


def make_spreadsheet(column_names, table):
    # column_names is a list of strings
    # table is a 2D numpy.ndarray
    # returns a vtkTable object that stores the table content

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
    Returns the shape of the output array for scipy.ndimage.interpolation.rotate
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

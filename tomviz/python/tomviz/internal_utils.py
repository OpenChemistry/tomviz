# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
import numpy as np
from tomviz._internal import in_application
from tomviz._internal import require_internal_mode
from tomviz._internal import with_vtk_dataobject
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

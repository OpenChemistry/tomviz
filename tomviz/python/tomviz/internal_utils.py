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

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

def get_array(dataobject):
    scalars_array = get_scalars(dataobject)
    scalars_array3d = np.reshape(scalars_array, (dataobject.GetDimensions()),
                                 order='F')
    return scalars_array3d

def set_array(dataobject, newarray):
    # Ensure we have Fortran ordered flat array to assign to image data. This
    # is ideally done without additional copies, but if C order we must copy.
    if np.isfortran(newarray):
        arr = newarray.reshape(-1, order='F')
    else:
        print 'Warning, new array did not have Fortran order, deep copy needed.'
        tmp = np.asfortranarray(newarray)
        arr = tmp.reshape(-1, order='F')

    # Set the extents (they may have changed).
    dataobject.SetExtent(0, newarray.shape[0] - 1,
                         0, newarray.shape[1] - 1,
                         0, newarray.shape[2] - 1)

    # Now replace the scalars array with the new array.
    vtkarray = np_s.numpy_to_vtk(arr)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    do = dsa.WrapDataObject(dataobject)
    oldscalars = do.PointData.GetScalars()
    name = oldscalars.GetName()
    del oldscalars
    do.PointData.append(arr, name)
    do.PointData.SetActiveScalars(name)

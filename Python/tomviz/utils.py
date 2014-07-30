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

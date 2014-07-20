import numpy as np
import vtk.numpy_interface.dataset_adapter as dsa

def get_scalars(dataobject):
    do = dsa.WrapDataObject(dataobject)
    # get the first
    rawarray = do.PointData.GetScalars()
    vtkarray = dsa.vtkDataArrayToVTKArray(rawarray, do)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    return vtkarray


def set_scalars(dataobject, scalars):
    do = dsa.WrapDataObject(dataobject)
    raise RuntimeError, "TODO"

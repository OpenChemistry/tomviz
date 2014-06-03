from paraview.vtk import dataset_adapter
import numpy as np

def getscalars(dataobject):
    do = dataset_adapter.WrapDataObject(dataobject)
    # get the first
    vtkarray = do.PointData.GetScalars()
    #print vtkarray.GetValue(0)
    nparray = dataset_adapter.vtkDataArrayToVTKArray(vtkarray)
    # since dataset_adapter returns a matrix (currently), we convert it to an
    # array.
    nparray = np.squeeze(np.asarray(nparray))
    try:
      # now reshape it based on the image extents.
      dims = do.GetDimensions()
      nparray = nparray.reshape(dims)
    except:
      pass
    #np.cos(nparray, nparray)
    #print vtkarray.GetValue(0)
    return nparray

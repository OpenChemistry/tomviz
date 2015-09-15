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
        print 'Warning, array does not have Fortran order, making deep copy and fixing...'
        tmp = np.asfortranarray(newarray)
        arr = tmp.reshape(-1, order='F')
        print '...done.'

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

def make_dataset(x, y, z, generate_data_function):
    from vtk import vtkImageData, VTK_DOUBLE
    array = np.zeros((x,y,z), order='F')
    generate_data_function(array)
    dataset = vtkImageData()
    dataset.SetOrigin(0,0,0)
    dataset.SetSpacing(1,1,1)
    dataset.SetExtent(0, x-1, 0, y-1, 0, z-1)
    flat_array = array.reshape(-1, order='F')
    vtkarray = np_s.numpy_to_vtk(flat_array, deep=1, array_type=VTK_DOUBLE)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    do = dsa.WrapDataObject(dataset)
    do.PointData.append(vtkarray, "generated_scalars")
    do.PointData.SetActiveScalars("generated_scalars")
    return dataset

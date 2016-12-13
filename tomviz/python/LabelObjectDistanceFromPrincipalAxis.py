def transform_scalars(dataset, label_value=1, axis=0):

    import itk
    import vtk
    from tomviz import utils

    fd = dataset.GetFieldData()
    axis_array = fd.GetArray('PrincipalAxes')
    assert axis_array is not None, "Dataset does not have a PrincipalAxes field data array"
    assert axis_array.GetNumberOfTuples() == 3, "PrincipalAxes array requires 3 tuples"
    assert axis_array.GetNumberOfComponents() == 3, "PrincipalAxes array requires 3 components"
    assert axis >= 0 and axis <= 2, "Invalid axis. Must be in range [0, 2]."

    axis = axis_array.GetTuple(axis)

    center_array = fd.GetArray('Center')
    assert center_array is not None, "Dataset does not have a Center field data array"
    assert center_array.GetNumberOfTuples() == 1, "Center array requires 1 tuple"
    assert center_array.GetNumberOfComponents() == 3, "Center array requires 3 components"

    center = center_array.GetTuple(0)

    import numpy as np

    # The funny ordering is to match VTK's convention for point storage
    #yy, xx, zz = np.meshgrid(y, x, z)
    xx, yy, zz = utils.get_coordinate_arrays(dataset)
    amp_x = center[0] - xx
    amp_y = center[1] - yy
    amp_z = center[2] - zz

    dot = (amp_x * axis[0]) + (amp_y * axis[1]) + (amp_z * axis[2])
    distance = np.sqrt(np.square(amp_x - (dot * axis[0])) +
                       np.square(amp_y - (dot * axis[1])) +
                       np.square(amp_z - (dot * axis[2])))

    # Check label values and store the distance only for those with labels > 0,
    # which represent non-background values.
    labels = utils.get_array(dataset)
    distance[labels != label_value] = 0

    import vtk.util.numpy_support as np_s
    distance_array = np_s.numpy_to_vtk(distance.reshape(-1, order='F'), deep=1)
    distance_array.SetName('Distance')
    dataset.GetPointData().SetScalars(distance_array)

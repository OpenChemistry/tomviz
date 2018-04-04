def transform_scalars(dataset, label_value=1):
    """Computes the principal axes of an object with the label value passed in
    as the parameter label_value. The principal axes are added to the field
    data of the dataset as an array of 3-component tuples named 'PrincipalAxes'.
    The principal axis tuples are stored in order of length from longest to
    shortest. The center of the object is also added to the field data as an
    array named 'Center' with a single 3-component tuple.

    This operator uses principal components analysis of the labeled point
    locations to determine the principal axes.
    """

    from tomviz import utils
    from vtkmodules.vtkCommonCore import vtkFloatArray
    from vtkmodules.vtkCommonDataModel import vtkDataSet

    fd = dataset.GetFieldData()

    (axes, center) = utils.label_object_principal_axes(dataset, label_value)
    print(axes)
    print(center)
    axis_array = vtkFloatArray()
    axis_array.SetName('PrincipalAxes')
    axis_array.SetNumberOfComponents(3)
    axis_array.SetNumberOfTuples(3)
    axis_array.InsertTypedTuple(0, list(axes[:, 0]))
    axis_array.InsertTypedTuple(1, list(axes[:, 1]))
    axis_array.InsertTypedTuple(2, list(axes[:, 2]))
    fd.RemoveArray('PrincipalAxis')
    fd.AddArray(axis_array)

    center_array = vtkFloatArray()
    center_array.SetName('Center')
    center_array.SetNumberOfComponents(3)
    center_array.SetNumberOfTuples(1)
    center_array.InsertTypedTuple(0, list(center))
    fd.RemoveArray('Center')
    fd.AddArray(center_array)

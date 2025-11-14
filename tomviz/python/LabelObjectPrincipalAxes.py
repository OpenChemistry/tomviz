def transform(dataset, label_value=1):
    """Computes the principal axes of an object with the label value passed in
    as the parameter label_value. The principal axes are added to the field
    data of the dataset as an array of 3-component tuples named 'PrincipalAxes'.
    The principal axis tuples are stored in order of length from longest to
    shortest. The center of the object is also added to the field data as an
    array named 'Center' with a single 3-component tuple.

    This operator uses principal components analysis of the labeled point
    locations to determine the principal axes.
    """

    from tomviz import itkutils

    (axes, center) = itkutils.label_object_principal_axes(dataset, label_value)

    print('Axes is:', axes)
    print('Center is:', center)

    # These set the field data on the vtkDataObject
    itkutils.set_principal_axes(dataset, axes)
    itkutils.set_center(dataset, center)

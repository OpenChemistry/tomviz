def principal_axes(dataset, label_value):
    import numpy as np
    from tomviz import utils
    labels = utils.get_array(dataset)
    num_voxels = np.sum(labels == label_value)
    xx, yy, zz = utils.get_coordinate_arrays(dataset)

    data = np.zeros((num_voxels, 3))
    selection = labels == label_value
    data[:, 0] = xx[selection]
    data[:, 1] = yy[selection]
    data[:, 2] = zz[selection]

    # Compute PCA on coordinates
    from scipy import linalg as la
    m, n = data.shape
    center = data.mean(axis=0)
    data -= center
    R = np.cov(data, rowvar=False)
    evals, evecs = la.eigh(R)
    idx = np.argsort(evals)[::-1]
    evecs = evecs[:, idx]
    evals = evals[idx]
    return (evecs, center)


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

    import vtk

    fd = dataset.GetFieldData()

    (axes, center) = principal_axes(dataset, label_value)
    print(axes)
    print(center)
    axis_array = vtk.vtkFloatArray()
    axis_array.SetName('PrincipalAxes')
    axis_array.SetNumberOfComponents(3)
    axis_array.SetNumberOfTuples(3)
    axis_array.InsertTypedTuple(0, list(axes[:, 0]))
    axis_array.InsertTypedTuple(1, list(axes[:, 1]))
    axis_array.InsertTypedTuple(2, list(axes[:, 2]))
    fd.RemoveArray('PrincipalAxis')
    fd.AddArray(axis_array)

    center_array = vtk.vtkFloatArray()
    center_array.SetName('Center')
    center_array.SetNumberOfComponents(3)
    center_array.SetNumberOfTuples(1)
    center_array.InsertTypedTuple(0, list(center))
    fd.RemoveArray('Center')
    fd.AddArray(center_array)

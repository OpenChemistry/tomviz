# -*- coding: utf-8 -*-

###############################################################################
#
#  This source file is part of the tomviz project.
#
#  Copyright Kitware, Inc.
#
#  This source code is released under the New BSD License, (the "License").
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###############################################################################

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


def set_array(dataobject, newarray, minextent=None):
    # Ensure we have Fortran ordered flat array to assign to image data. This
    # is ideally done without additional copies, but if C order we must copy.
    if np.isfortran(newarray):
        arr = newarray.reshape(-1, order='F')
    else:
        print ('Warning, array does not have Fortran order, making deep copy '
               'and fixing...')
        tmp = np.asfortranarray(newarray)
        arr = tmp.reshape(-1, order='F')
        print '...done.'

    # Set the extent if needed, i.e. if the minextent is not the same as
    # the data object starting index, or if the newarray shape is not the same
    # as the size of the dataobject.
    if minextent is None:
        minextent = dataobject.GetExtent()[::2]
    sameindex = list(minextent) == list(dataobject.GetExtent()[::2])
    sameshape = list(newarray.shape) == list(dataobject.GetDimensions())
    if not sameindex or not sameshape:
        extent = 6*[0]
        extent[::2] = minextent
        extent[1::2] = \
            [x + y - 1 for (x, y) in zip(minextent, newarray.shape)]
        dataobject.SetExtent(extent)

    # Now replace the scalars array with the new array.
    vtkarray = np_s.numpy_to_vtk(arr)
    vtkarray.Association = dsa.ArrayAssociation.POINT
    do = dsa.WrapDataObject(dataobject)
    oldscalars = do.PointData.GetScalars()
    arrayname = "Scalars"
    if oldscalars is not None:
        arrayname = oldscalars.GetName()
    del oldscalars
    do.PointData.append(arr, arrayname)
    do.PointData.SetActiveScalars(arrayname)


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


def get_coordinate_arrays(dataset):
    """Returns a triple of Numpy arrays containing x, y, and z coordinates for
    each point in the dataset. This can be used to evaluate a function at each
    point, for instance.
    """
    assert dataset.IsA("vtkImageData"), "Dataset must be a vtkImageData"

    # Create meshgrid for image
    spacing = dataset.GetSpacing()
    origin = dataset.GetOrigin()
    dims = dataset.GetDimensions()
    x = [origin[0] + (spacing[0] * i) for i in range(dims[0])]
    y = [origin[1] + (spacing[1] * i) for i in range(dims[1])]
    z = [origin[2] + (spacing[2] * i) for i in range(dims[2])]

    # The funny ordering is to match VTK's convention for point storage
    yy, xx, zz = np.meshgrid(y, x, z)

    return (xx, yy, zz)


def label_object_principal_axes(dataset, label_value):
    import numpy as np
    from tomviz import utils
    labels = utils.get_array(dataset)
    num_voxels = np.sum(labels == label_value)
    xx, yy, zz = utils.get_coordinate_arrays(dataset)

    data = np.zeros((num_voxels, 3))
    selection = labels == label_value
    assert np.any(selection), \
        "No voxels with label %d in label map" % label_value
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


def make_dataset(x, y, z, dataset, generate_data_function, **kwargs):
    from vtk import VTK_DOUBLE
    array = np.zeros((x, y, z), order='F')
    generate_data_function(array, **kwargs)
    dataset.SetOrigin(0, 0, 0)
    dataset.SetSpacing(1, 1, 1)
    dataset.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
    flat_array = array.reshape(-1, order='F')
    vtkarray = np_s.numpy_to_vtk(flat_array, deep=1, array_type=VTK_DOUBLE)
    vtkarray.SetName("generated_scalars")
    dataset.GetPointData().SetScalars(vtkarray)


def mark_as_volume(dataobject):
    from vtk import vtkTypeInt8Array
    fd = dataobject.GetFieldData()
    arr = fd.GetArray("tomviz_data_source_type")
    if arr is None:
        arr = vtkTypeInt8Array()
        arr.SetNumberOfComponents(1)
        arr.SetNumberOfTuples(1)
        arr.SetName("tomviz_data_source_type")
        fd.AddArray(arr)
    arr.SetTuple1(0, 0)


def mark_as_tiltseries(dataobject):
    from vtk import vtkTypeInt8Array
    fd = dataobject.GetFieldData()
    arr = fd.GetArray("tomviz_data_source_type")
    if arr is None:
        arr = vtkTypeInt8Array()
        arr.SetNumberOfComponents(1)
        arr.SetNumberOfTuples(1)
        arr.SetName("tomviz_data_source_type")
        fd.AddArray(arr)
    arr.SetTuple1(0, 1)


def make_spreadsheet(column_names, table):
    # column_names is a list of strings
    # table is a 2D numpy.ndarray
    # returns a vtkTable object that stores the table content

    # Create a vtkTable to store the output.
    rows = table.shape[0]

    if (table.shape[1] != len(column_names)):
        print('Warning: table number of columns differs from number of '
              'column names')
        return

    from vtk import vtkTable, vtkFloatArray
    vtk_table = vtkTable()
    for (column, name) in enumerate(column_names):
        array = vtkFloatArray()
        array.SetName(name)
        array.SetNumberOfComponents(1)
        array.SetNumberOfTuples(rows)
        vtk_table.AddColumn(array)

        for row in xrange(0, rows):
            array.InsertValue(row, table[row, column])

    return vtk_table

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

# Dictionary going from VTK array type to ITK type
_vtk_to_itk_types = None


def vtk_itk_map():
    """Try to set up mappings between VTK image types and ITK image types.
    Not all ITK image types may be available, hence the try statements."""
    global _vtk_to_itk_types

    if _vtk_to_itk_types is None:
        import itk
        _vtk_to_itk_types = {}

        type_map = {
            'vtkUnsignedCharArray': 'UC3',
            'vtkCharArray': 'SC3',
            'vtkUnsignedShortArray': 'US3',
            'vtkShortArray': 'SS3',
            'vtkUnsignedIntArray': 'UI3',
            'vtkIntArray': 'SI3',
            'vtkFloatArray': 'F3',
            'vtkDoubleArray': 'D3'
        }

        for (vtk_type, image_type) in type_map.iteritems():
            try:
                _vtk_to_itk_types[vtk_type] = getattr(itk.Image, image_type)
            except AttributeError:
                pass

    return _vtk_to_itk_types


def get_itk_image_type(vtk_image_data):
    """
    Get an ITK image type corresponding to the provided vtkImageData object.
    """
    image_type = None

    # Get the scalars
    pd = vtk_image_data.GetPointData()
    scalars = pd.GetScalars()

    vtk_class_name = scalars.GetClassName()

    try:
        image_type = vtk_itk_map()[vtk_class_name]
    except KeyError:
        raise Exception('No ITK type known for %s' % vtk_class_name)

    return image_type


def convert_vtk_to_itk_image(vtk_image_data):
    """Get an ITK image from the provided vtkImageData object.
    This image can be passed to ITK filters."""
    image_type = get_itk_image_type(vtk_image_data)

    # Save the VTKGlue optimization for later
    #------------------------------------------
    #itk_import = itk.VTKImageToImageFilter[image_type].New()
    #itk_import.SetInput(vtk_image_data)
    #itk_import.Update()
    #itk_image = itk_import.GetOutput()
    #itk_image.DisconnectPipeline()
    #------------------------------------------
    import itk
    array = get_array(vtk_image_data)
    itk_converter = itk.PyBuffer[image_type]
    itk_image = itk_converter.GetImageFromArray(array)

    return itk_image


def add_vtk_array_from_itk_image(itk_image_data, vtk_image_data, name):
    """Add an array from an ITK image to a vtkImageData with a given name."""

    itk_output_image_type = type(itk_image_data)

    # Save the VTKGlue optimization for later
    #------------------------------------------
    # Export the ITK image to a VTK image. No copying should take place.
    #export_filter = itk.ImageToVTKImageFilter[itk_output_image_type].New()
    #export_filter.SetInput(itk_image_data)
    #export_filter.Update()

    # Get scalars from the temporary image and copy them to the data set
    #result_image = export_filter.GetOutput()
    #filter_array = result_image.GetPointData().GetArray(0)

    # Make a new instance of the array that will stick around after this
    # filters in this script are garbage collected
    #new_array = filter_array.NewInstance()
    #new_array.DeepCopy(filter_array) # Should be able to shallow copy?
    #new_array.SetName(name)

    # Set a new point data array in the dataset
    #vtk_image_data.GetPointData().AddArray(new_array)
    #------------------------------------------
    import itk
    result = itk.PyBuffer[
        itk_output_image_type].GetArrayFromImage(itk_image_data)
    set_label_map(vtk_image_data, result)


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
        print ('Warning, array does not have Fortran order, making deep copy '
               'and fixing...')
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


def set_label_map(dataobject, labelarray):
    # Ensure we have Fortran ordered flat array to assign to image data. This
    # is ideally done without additional copies, but if C order we must copy.
    if np.isfortran(labelarray):
        arr = labelarray.reshape(-1, order='F')
    else:
        print ('Warning, array does not have Fortran order, making deep copy '
               'and fixing...')
        tmp = np.asfortranarray(labelarray)
        arr = tmp.reshape(-1, order='F')
        print '...done.'

    # Now add the label array to the image data
    do = dsa.WrapDataObject(dataobject)
    do.PointData.append(arr, "LabelMap")
    pd = dataobject.GetPointData()
    pd.SetScalars(pd.GetArray("LabelMap"))


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


def make_dataset(x, y, z, dataset, generate_data_function):
    from vtk import VTK_DOUBLE
    array = np.zeros((x, y, z), order='F')
    generate_data_function(array)
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

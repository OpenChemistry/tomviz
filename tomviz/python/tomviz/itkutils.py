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
from tomviz import py2to3

# Dictionary going from VTK array type to ITK type
_vtk_to_itk_types = None

# Dictionary mapping from VTK array numeric type to the VTK array numeric type
# supported by the ITK wrapping. Used to cast data sets to a supported type
# prior to converting them to ITK images.
_vtk_cast_types = None

# Dictionary mapping from ITK image type to Python numeric type.
# Used for casting Python values to a suitable type for certain filters.
_itkctype_to_python_types = None

# Map between VTK numeric type and Python numeric type
_vtk_to_python_types = None


def vtk_itk_type_map():
    """Set up mappings between VTK image types and available ITK image
    types."""
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

        for (vtk_type, image_type) in py2to3.iteritems(type_map):
            try:
                _vtk_to_itk_types[vtk_type] = getattr(itk.Image, image_type)
            except AttributeError:
                pass

    return _vtk_to_itk_types


def vtk_cast_map():
    """Set up mapping between VTK array numeric types to VTK numeric types
    that correspond to supported ITK numeric ctypes supported by the ITK
    wrapping."""
    global _vtk_cast_types

    if _vtk_cast_types is None:
        import itk
        _vtk_cast_types = {}

        # Map from VTK array type to list of possible types to which they can be
        # converted, listed in order of preference based on representability
        # and memory size required.
        import vtk
        type_map = {
            vtk.VTK_UNSIGNED_CHAR: [
                'unsigned char',
                'unsigned short',
                'unsigned int',
                'unsigned long',
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_CHAR: [
                'signed char',
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_SIGNED_CHAR: [
                'signed char',
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_UNSIGNED_SHORT: [
                'unsigned short',
                'unsigned int',
                'unsigned long',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_SHORT: [
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_UNSIGNED_INT: [
                'unsigned int',
                'unsigned long',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_INT: [
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtk.VTK_FLOAT: [
                'float',
                'double'
            ],
            vtk.VTK_DOUBLE: [
                'double',
                'float'
            ]
        }

        # Map ITK ctype back to VTK type
        ctype_to_vtk = {
            'unsigned char': vtk.VTK_UNSIGNED_CHAR,
            'signed char': vtk.VTK_CHAR,
            'unsigned short': vtk.VTK_UNSIGNED_SHORT,
            'signed short': vtk.VTK_SHORT,
            'unsigned int': vtk.VTK_UNSIGNED_INT,
            'signed int': vtk.VTK_INT,
            'unsigned long': vtk.VTK_UNSIGNED_LONG,
            'signed long': vtk.VTK_LONG,
            'float': vtk.VTK_FLOAT,
            'double': vtk.VTK_DOUBLE
        }

        # Import build options from ITK. Explicitly reference itk.Vector so
        # that the itk::Vector type information is available when
        # itk.BuildOptions is imported, otherwise we get an exception when
        # importing itkBuildOptions. This is a workaround for a bug in ITK.
        itk.Vector
        import itkBuildOptions

        # Select the best supported type available in the wrapping.
        for (vtk_type, possible_image_types) in py2to3.iteritems(type_map):
            type_map[vtk_type] = None
            for possible_type in possible_image_types:
                if itk.ctype(possible_type) in itkBuildOptions.SCALARS:
                    _vtk_cast_types[vtk_type] = ctype_to_vtk[possible_type]
                    break

    return _vtk_cast_types


def get_python_voxel_type(dataset):
    """Return the Python type that can represent the voxel type in the dataset.
    The dataset can be either a VTK dataset or an ITK image.
    """

    # Try treating dataset as a VTK data set first.
    try:
        # Set up map between VTK data type and Python type
        global _vtk_to_python_types

        if _vtk_to_python_types is None:
            import vtk
            _vtk_to_python_types = {
                vtk.VTK_UNSIGNED_CHAR: int,
                vtk.VTK_CHAR: int,
                vtk.VTK_UNSIGNED_SHORT: int,
                vtk.VTK_SHORT: int,
                vtk.VTK_UNSIGNED_INT: int,
                vtk.VTK_INT: int,
                vtk.VTK_UNSIGNED_LONG: int,
                vtk.VTK_LONG: int,
                vtk.VTK_FLOAT: float,
                vtk.VTK_DOUBLE: float
            }

        pd = dataset.GetPointData()
        scalars = pd.GetScalars()

        return _vtk_to_python_types[scalars.GetDataType()]
    except AttributeError as attribute_error:
        pass

    # If the above fails, treat dataset as an ITK image.
    try:
        # Set up map between ITK ctype and Python type.
        global _itkctype_to_python_types

        if _itkctype_to_python_types is None:
            import itkTypes

            _itkctype_to_python_types = {
                itkTypes.F: float,
                itkTypes.D: float,
                itkTypes.LD: float,
                itkTypes.UC: int,
                itkTypes.US: int,
                itkTypes.UI: int,
                itkTypes.UL: int,
                itkTypes.SC: int,
                itkTypes.SS: int,
                itkTypes.SI: int,
                itkTypes.SL: int,
                itkTypes.B: int
            }

        import itkExtras

        # Incantation for obtaining voxel type in ITK image
        ctype = itkExtras.template(type(dataset))[1][0]
        return _itkctype_to_python_types[ctype]
    except AttributeError as attribute_error:
        print("Could not get Python voxel type for dataset %s"
              % type(dataset))
        print(attribute_error)


def convert_vtk_to_itk_image(vtk_image_data, itk_pixel_type=None):
    """Get an ITK image from the provided vtkImageData object.
    This image can be passed to ITK filters."""

    # Save the VTKGlue optimization for later
    #------------------------------------------
    #itk_import = itk.VTKImageToImageFilter[image_type].New()
    #itk_import.SetInput(vtk_image_data)
    #itk_import.Update()
    #itk_image = itk_import.GetOutput()
    #itk_image.DisconnectPipeline()
    #------------------------------------------
    import itk
    import itkTypes
    import vtk
    from tomviz import utils

    itk_to_vtk_type_map = {
        itkTypes.F: vtk.VTK_FLOAT,
        itkTypes.D: vtk.VTK_DOUBLE,
        itkTypes.LD: vtk.VTK_DOUBLE,
        itkTypes.UC: vtk.VTK_UNSIGNED_CHAR,
        itkTypes.US: vtk.VTK_UNSIGNED_SHORT,
        itkTypes.UI: vtk.VTK_UNSIGNED_INT,
        itkTypes.UL: vtk.VTK_UNSIGNED_LONG,
        itkTypes.SC: vtk.VTK_CHAR,
        itkTypes.SS: vtk.VTK_SHORT,
        itkTypes.SI: vtk.VTK_INT,
        itkTypes.SL: vtk.VTK_LONG,
        itkTypes.B: vtk.VTK_INT
    }

    # See if we need to cast to a wrapped type in ITK.
    src_type = vtk_image_data.GetScalarType()

    if itk_pixel_type is None:
        dst_type = vtk_cast_map()[src_type]
    else:
        dst_type = vtk_cast_map()[itk_to_vtk_type_map[itk_pixel_type]]
    if src_type != dst_type:
        caster = vtk.vtkImageCast()
        caster.SetOutputScalarType(dst_type)
        caster.ClampOverflowOn()
        caster.SetInputData(vtk_image_data)
        caster.Update()
        vtk_image_data = caster.GetOutput()

    array = utils.get_array(vtk_image_data, order='C')

    image_type = _get_itk_image_type(vtk_image_data)
    itk_converter = itk.PyBuffer[image_type]
    itk_image = itk_converter.GetImageFromArray(array)
    spacing = vtk_image_data.GetSpacing()
    origin = vtk_image_data.GetOrigin()
    itk_image.SetSpacing(spacing)
    itk_image.SetOrigin(origin)

    # Persist a reference to the source vtk_image_data, which is necessary since
    # VTK and ITK are using Python Buffer-Protocol NumPy array views
    itk_image.vtk_image_data = vtk_image_data

    return itk_image


def set_array_from_itk_image(dataset, itk_image):
    """Set dataset array from an ITK image."""

    itk_output_image_type = type(itk_image)

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
    #------------------------------------------
    import itk
    from . import utils
    result = itk.PyBuffer[
        itk_output_image_type].GetArrayFromImage(itk_image)
    result = result.copy()
    utils.set_array(dataset, result, isFortran=False)


def get_label_object_attributes(dataset, progress_callback=None):
    """Compute shape attributes of integer-labeled objects in a dataset. Returns
    an ITK shape label map. An optional progress_callback function can be passed
    in. This callback is expected to take one argument, a floating-point number
    in the range [0, 1] that represents the progress amount. It returns a value
    indicating whether the caller should be cancelled.
    """

    try:
        import itk

        # Get an ITK image from the data set
        itk_image = convert_vtk_to_itk_image(dataset)
        itk_image_type = type(itk_image)

        # Get an appropriate LabelImageToShapelLabelMapFilter type for the
        # input.
        inputTypes = [x[0] for x in list(itk.LabelImageToShapeLabelMapFilter.keys())] # noqa
        filterTypeIndex = inputTypes.index(itk_image_type)
        if filterTypeIndex < 0:
            raise Exception("No suitable filter type for input type %s" %
                            type(itk_image_type))

        # Now take the connected components results and compute things like
        # volume and surface area.
        shape_filter = \
            list(itk.LabelImageToShapeLabelMapFilter.values())[filterTypeIndex].New() # noqa
        shape_filter.SetInput(itk_image)

        def progress_func():
            progress = shape_filter.GetProgress()
            if progress_callback is not None:
                abort = progress_callback(progress)
                if abort:
                    shape_filter.AbortGenerateDataOn()

        progress_observer = itk.PyCommand.New()
        progress_observer.SetCommandCallable(progress_func)
        shape_filter.AddObserver(itk.ProgressEvent(), progress_observer)

        try:
            shape_filter.Update()
        except RuntimeError:
            return None

        label_map = shape_filter.GetOutput()
        return label_map
    except Exception as exc:
        print("Problem encountered while running label_object_attributes")
        raise(exc)


def _get_itk_image_type(vtk_image_data):
    """
    Get an ITK image type corresponding to the provided vtkImageData object.
    """
    image_type = None

    # Get the scalars
    pd = vtk_image_data.GetPointData()
    scalars = pd.GetScalars()

    vtk_class_name = scalars.GetClassName()

    try:
        image_type = vtk_itk_type_map()[vtk_class_name]
    except KeyError:
        raise Exception('No ITK type known for %s' % vtk_class_name)

    return image_type


def observe_filter_progress(transform, filter, start_pct, end_pct):
    assert start_pct < end_pct
    pct_diff = end_pct - start_pct

    def progress_func():
        progress = filter.GetProgress()
        transform.progress.value = start_pct + int(progress * pct_diff)
        if transform.canceled:
            filter.AbortGenerateDataOn()

    import itk
    progress_observer = itk.PyCommand.New()
    progress_observer.SetCommandCallable(progress_func)
    filter.AddObserver(itk.ProgressEvent(), progress_observer)

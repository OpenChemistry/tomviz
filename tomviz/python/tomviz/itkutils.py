# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

from tomviz._internal import in_application
from tomviz._internal import require_internal_mode
from tomviz._internal import with_dataset
from tomviz._internal import with_vtk_dataobject

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

        for (vtk_type, image_type) in type_map.items():
            try:
                _vtk_to_itk_types[vtk_type] = getattr(itk.Image, image_type)
            except AttributeError:
                pass

    return _vtk_to_itk_types


def vtk_cast_map():
    """Set up mapping between VTK array numeric types to VTK numeric types
    that correspond to supported ITK numeric ctypes supported by the ITK
    wrapping."""
    require_internal_mode()

    global _vtk_cast_types

    if _vtk_cast_types is None:
        import itk
        _vtk_cast_types = {}

        # Map from VTK array type to list of possible types to which they can be
        # converted, listed in order of preference based on representability
        # and memory size required.
        from vtkmodules.util import vtkConstants
        type_map = {
            vtkConstants.VTK_UNSIGNED_CHAR: [
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
            vtkConstants.VTK_CHAR: [
                'signed char',
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtkConstants.VTK_SIGNED_CHAR: [
                'signed char',
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtkConstants.VTK_UNSIGNED_SHORT: [
                'unsigned short',
                'unsigned int',
                'unsigned long',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtkConstants.VTK_SHORT: [
                'signed short',
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtkConstants.VTK_UNSIGNED_INT: [
                'unsigned int',
                'unsigned long',
                'signed long',
                'float',
                'double'
            ],
            vtkConstants.VTK_INT: [
                'signed int',
                'signed long',
                'float',
                'double'
            ],
            vtkConstants.VTK_FLOAT: [
                'float',
                'double'
            ],
            vtkConstants.VTK_DOUBLE: [
                'double',
                'float'
            ]
        }

        # Map ITK ctype back to VTK type
        ctype_to_vtk = {
            'unsigned char': vtkConstants.VTK_UNSIGNED_CHAR,
            'signed char': vtkConstants.VTK_CHAR,
            'unsigned short': vtkConstants.VTK_UNSIGNED_SHORT,
            'signed short': vtkConstants.VTK_SHORT,
            'unsigned int': vtkConstants.VTK_UNSIGNED_INT,
            'signed int': vtkConstants.VTK_INT,
            'unsigned long': vtkConstants.VTK_UNSIGNED_LONG,
            'signed long': vtkConstants.VTK_LONG,
            'float': vtkConstants.VTK_FLOAT,
            'double': vtkConstants.VTK_DOUBLE
        }

        # Import build options from ITK. Explicitly reference itk.Vector so
        # that the itk::Vector type information is available when
        # itk.BuildOptions is imported, otherwise we get an exception when
        # importing itkBuildOptions. This is a workaround for a bug in ITK.
        itk.Vector
        import itkBuildOptions

        # Select the best supported type available in the wrapping.
        for (vtk_type, possible_image_types) in type_map.items():
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

    if in_application():
        # Try treating dataset as a VTK data set first.
        try:
            # Set up map between VTK data type and Python type
            global _vtk_to_python_types

            if _vtk_to_python_types is None:
                from vtkmodules.util import vtkConstants
                _vtk_to_python_types = {
                    vtkConstants.VTK_UNSIGNED_CHAR: int,
                    vtkConstants.VTK_CHAR: int,
                    vtkConstants.VTK_UNSIGNED_SHORT: int,
                    vtkConstants.VTK_SHORT: int,
                    vtkConstants.VTK_UNSIGNED_INT: int,
                    vtkConstants.VTK_INT: int,
                    vtkConstants.VTK_UNSIGNED_LONG: int,
                    vtkConstants.VTK_LONG: int,
                    vtkConstants.VTK_FLOAT: float,
                    vtkConstants.VTK_DOUBLE: float
                }

            pd = dataset.GetPointData()
            scalars = pd.GetScalars()

            return _vtk_to_python_types[scalars.GetDataType()]
        except AttributeError:
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


@with_vtk_dataobject
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
    from vtkmodules.util import vtkConstants
    from vtkmodules.vtkImagingCore import vtkImageCast
    from tomviz import internal_utils

    itk_to_vtk_type_map = {
        itkTypes.F: vtkConstants.VTK_FLOAT,
        itkTypes.D: vtkConstants.VTK_DOUBLE,
        itkTypes.LD: vtkConstants.VTK_DOUBLE,
        itkTypes.UC: vtkConstants.VTK_UNSIGNED_CHAR,
        itkTypes.US: vtkConstants.VTK_UNSIGNED_SHORT,
        itkTypes.UI: vtkConstants.VTK_UNSIGNED_INT,
        itkTypes.UL: vtkConstants.VTK_UNSIGNED_LONG,
        itkTypes.SC: vtkConstants.VTK_CHAR,
        itkTypes.SS: vtkConstants.VTK_SHORT,
        itkTypes.SI: vtkConstants.VTK_INT,
        itkTypes.SL: vtkConstants.VTK_LONG,
        itkTypes.B: vtkConstants.VTK_INT
    }

    # See if we need to cast to a wrapped type in ITK.
    src_type = vtk_image_data.GetScalarType()

    if itk_pixel_type is None:
        dst_type = vtk_cast_map()[src_type]
    else:
        dst_type = vtk_cast_map()[itk_to_vtk_type_map[itk_pixel_type]]
    if src_type != dst_type:
        caster = vtkImageCast()
        caster.SetOutputScalarType(dst_type)
        caster.ClampOverflowOn()
        caster.SetInputData(vtk_image_data)
        caster.Update()
        vtk_image_data = caster.GetOutput()

    array = internal_utils.get_array(vtk_image_data, order='C')

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


@with_vtk_dataobject
def set_array_from_itk_image(dataobject, itk_image):
    """Set dataobject array from an ITK image."""
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
    from . import internal_utils
    result = itk.PyBuffer[
        itk_output_image_type].GetArrayFromImage(itk_image)
    result = result.copy()
    internal_utils.set_array(dataobject, result, isFortran=False)


@with_dataset
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
        itk_image = dataset_to_itk_image(dataset)
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
        raise (exc)


@with_dataset
def label_object_principal_axes(dataset, label_value):
    import numpy as np

    labels = dataset.active_scalars
    num_voxels = np.sum(labels == label_value)
    xx, yy, zz = get_coordinate_arrays(dataset)

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




@with_dataset
def connected_components(dataset, background_value=0, progress_callback=None):
    try:
        import numpy as np
        import itk
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    if np.issubdtype(dataset.active_scalars.dtype, np.floating):
        raise Exception(
            "Connected Components works only on images with integral types.")

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image. The input is assumed to have an integral type.
        # Take care of casting to an unsigned short image so we can store up
        # to 65,535 connected components (the number of connected components
        # is limited to the maximum representable number in the voxel type
        # of the input image in the ConnectedComponentsFilter).
        array = dataset.active_scalars.astype(np.uint16)
        itk_image = itk.GetImageViewFromArray(array)
        itk_image.SetSpacing(dataset.spacing)
        itk_image_type = type(itk_image)

        # ConnectedComponentImageFilter
        connected_filter = itk.ConnectedComponentImageFilter[
            itk_image_type, itk_image_type].New()
        connected_filter.SetBackgroundValue(background_value)
        connected_filter.SetInput(itk_image)

        if progress_callback is not None:

            def connected_progress_func():
                progress = connected_filter.GetProgress()
                abort = progress_callback(progress * 0.5)
                connected_filter.SetAbortGenerateData(abort)

            connected_observer = itk.PyCommand.New()
            connected_observer.SetCommandCallable(connected_progress_func)
            connected_filter.AddObserver(itk.ProgressEvent(),
                                         connected_observer)

        # Relabel filter. This will compress the label numbers to a
        # continugous range between 1 and n where n is the number of
        # labels. It will also sort the components from largest to
        # smallest, where the largest component has label 1, the
        # second largest has label 2, and so on...
        relabel_filter = itk.RelabelComponentImageFilter[
            itk_image_type, itk_image_type].New()
        relabel_filter.SetInput(connected_filter.GetOutput())
        relabel_filter.SortByObjectSizeOn()

        if progress_callback is not None:

            def relabel_progress_func():
                progress = relabel_filter.GetProgress()
                abort = progress_callback(progress * 0.5 + 0.5)
                relabel_filter.SetAbortGenerateData(abort)

            relabel_observer = itk.PyCommand.New()
            relabel_observer.SetCommandCallable(relabel_progress_func)
            relabel_filter.AddObserver(itk.ProgressEvent(), relabel_observer)

        try:
            relabel_filter.Update()
        except RuntimeError:
            return

        itk_image_data = relabel_filter.GetOutput()
        label_buffer = itk.PyBuffer[
            itk_image_type].GetArrayFromImage(itk_image_data)

        # Flip the labels so that the largest component has the highest label
        # value, e.g., the labeling ordering by size goes from [1, 2, ... N] to
        # [N, N-1, N-2, ..., 1]. Note that zero is the background value, so we
        # do not want to change it.
        import numpy as np
        minimum = 1  # Minimum label is always 1, background is 0
        maximum = np.max(label_buffer)

        # Try more memory-efficient approach
        gt_zero = label_buffer > 0
        label_buffer[gt_zero] = minimum - label_buffer[gt_zero] + maximum

        # Transpose the data to Fortran indexing
        dataset.active_scalars = label_buffer.transpose([2, 1, 0])

    except Exception as exc:
        print("Problem encountered while running ConnectedComponents")
        raise exc


@with_vtk_dataobject
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


@with_dataset
def dataset_to_itk_image(dataset):

    import itk

    itk_image = itk.GetImageViewFromArray(dataset.active_scalars)

    if dataset.spacing is not None:
        itk_image.SetSpacing(dataset.spacing)

    return itk_image


def set_itk_image_on_dataset(itk_image, dataset, dtype=None):
    # Write the itk image data to the dataset

    import itk

    if dtype is None:
        array = itk.GetArrayFromImage(itk_image)
    else:
        array = itk.PyBuffer[dtype].GetArrayFromImage(itk_image)

    # Transpose the data to Fortran indexing
    dataset.active_scalars = array.transpose([2, 1, 0])


@with_vtk_dataobject
def set_principal_axes(dataobject, axes):
    from vtkmodules.vtkCommonCore import vtkFloatArray

    fd = dataobject.GetFieldData()

    axis_array = vtkFloatArray()
    axis_array.SetName('PrincipalAxes')
    axis_array.SetNumberOfComponents(3)
    axis_array.SetNumberOfTuples(3)
    axis_array.InsertTypedTuple(0, list(axes[:, 0]))
    axis_array.InsertTypedTuple(1, list(axes[:, 1]))
    axis_array.InsertTypedTuple(2, list(axes[:, 2]))
    fd.RemoveArray('PrincipalAxis')
    fd.AddArray(axis_array)


@with_vtk_dataobject
def get_principal_axes(dataobject, principal_axis):
    fd = dataobject.GetFieldData()
    axis_array = fd.GetArray('PrincipalAxes')
    assert axis_array is not None, \
        "Dataset does not have a PrincipalAxes field data array"
    assert axis_array.GetNumberOfTuples() == 3, \
        "PrincipalAxes array requires 3 tuples"
    assert axis_array.GetNumberOfComponents() == 3, \
        "PrincipalAxes array requires 3 components"
    assert principal_axis >= 0 and principal_axis <= 2, \
        "Invalid principal axis. Must be in range [0, 2]."

    return np.array(axis_array.GetTuple(principal_axis))


@with_vtk_dataobject
def set_center(dataobject, center):
    from vtkmodules.vtkCommonCore import vtkFloatArray

    fd = dataobject.GetFieldData()

    center_array = vtkFloatArray()
    center_array.SetName('Center')
    center_array.SetNumberOfComponents(3)
    center_array.SetNumberOfTuples(1)
    center_array.InsertTypedTuple(0, list(center))
    fd.RemoveArray('Center')
    fd.AddArray(center_array)


@with_vtk_dataobject
def get_center(dataobject):
    fd = dataobject.GetFieldData()
    center_array = fd.GetArray('Center')
    assert center_array is not None, \
        "Dataset does not have a Center field data array"
    assert center_array.GetNumberOfTuples() == 1, \
        "Center array requires 1 tuple"
    assert center_array.GetNumberOfComponents() == 3, \
        "Center array requires 3 components"

    return np.array(center_array.GetTuple(0))

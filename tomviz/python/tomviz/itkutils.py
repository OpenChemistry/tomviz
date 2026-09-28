# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################

from tomviz._internal import in_application
from tomviz._internal import with_dataset
from tomviz._internal import with_vtk_dataobject

# Dictionary mapping from ITK image type to Python numeric type.
# Used for casting Python values to a suitable type for certain filters.
_itkctype_to_python_types = None

# Map between VTK numeric type and Python numeric type
_vtk_to_python_types = None


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
            import itk

            _itkctype_to_python_types = {
                itk.F: float,
                itk.D: float,
                itk.LD: float,
                itk.UC: int,
                itk.US: int,
                itk.UI: int,
                itk.UL: int,
                itk.SC: int,
                itk.SS: int,
                itk.SI: int,
                itk.SL: int,
                itk.B: int
            }

        import itk

        # Incantation for obtaining voxel type in ITK image
        ctype = itk.template(type(dataset))[1][0]
        return _itkctype_to_python_types[ctype]
    except AttributeError as attribute_error:
        print("Could not get Python voxel type for dataset %s"
              % type(dataset))
        print(attribute_error)


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
        label_buffer = itk.GetArrayFromImage(itk_image_data)

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


def set_itk_image_on_dataset(itk_image, dataset, **kwargs):
    # Write the itk image data to the dataset

    import itk

    array = itk.GetArrayFromImage(itk_image)

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

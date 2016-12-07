def transform_scalars(dataset):
    """This filter generates a label map of connected components of foreground
    voxels in the input image. Foreground voxels have non-zero values. Input
    images are expected to have integral voxel types, i.e., no float or
    double voxels.
    """

    try:
        import itk
        import itkTypes
        import vtk
        from tomviz import itkutils
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    background_value = 0

    # Return values
    returnValues = None

    scalarType = dataset.GetScalarType()
    if scalarType == vtk.VTK_FLOAT or scalarType == vtk.VTK_DOUBLE:
        raise Exception(
            "Connected Components works only on images with integral types.")

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image. The input is assumed to have an integral type.
        # Take care of casting to an unsigned short image so we can store up
        # to 65,535 connected components (the number of connected components
        # is limited to the maximum representable number in the input image
        # voxel type).
        itk_image = itkutils.convert_vtk_to_itk_image(dataset, itkTypes.US)
        itk_image_type = type(itk_image)

        # ConnectedComponentImageFilter
        connected_filter = itk.ConnectedComponentImageFilter[
            itk_image_type, itk_image_type].New()
        connected_filter.SetBackgroundValue(background_value)
        connected_filter.SetInput(itk_image)

        # Relabel filter. This will compress the label numbers to a
        # continugous range between 1 and n where n is the number of
        # labels. It will also sort the components from largest to
        # smallest, where the largest component has label 1, the
        # second largest has label 2, and so on...
        relabel_filter = itk.RelabelComponentImageFilter[
            itk_image_type, itk_image_type].New()
        relabel_filter.SetInput(connected_filter.GetOutput())
        relabel_filter.SortByObjectSizeOn()
        relabel_filter.Update()

        itk_image_data = relabel_filter.GetOutput()
        label_buffer = itk.PyBuffer[
            itk_image_type].GetArrayFromImage(itk_image_data)

        # Flip the labels so that the largest component has the highest label
        # value, e.g., the labeling ordering by size goes from [1, 2, ... N] to
        # [N, N-1, N-2, ..., 1]. Note that zero is the background value, so we
        # do not want to change it.
        import numpy as np
        minimum = 1 # Minimum label is always 1, background is 0
        maximum = np.max(label_buffer)

        # Try more memory-efficient approach
        gt_zero = label_buffer > 0
        label_buffer[gt_zero] = minimum - label_buffer[gt_zero] + maximum

        utils.set_array(dataset, label_buffer)

        # Now take the connected components results and compute things like
        # volume and surface area.
        shape_filter = itk.LabelImageToShapeLabelMapFilter.IUS3LM3.New()
        shape_filter.SetInput(relabel_filter.GetOutput())
        shape_filter.Update()

        # Set up arrays to hold the shape attribute data
        label_map = shape_filter.GetOutput()
        num_label_objects = label_map.GetNumberOfLabelObjects()

        column_names = ['SurfaceArea', 'Volume', 'SurfaceAreaToVolumeRatio']
        import numpy as np
        # num_label_objects rows, 3 columns
        table = np.zeros((num_label_objects, len(column_names)))

        for i in xrange(0, num_label_objects):
            label_object = label_map.GetNthLabelObject(i)
            surface_area = label_object.GetPerimeter()
            table[i, 0] = surface_area
            volume = label_object.GetPhysicalSize()
            table[i, 1] = volume
            table[i, 2] = surface_area / volume

        # Create a spreadsheet data set from table data
        spreadsheet = utils.make_spreadsheet(column_names, table)

        # Set up dictionary to return operator results
        returnValues = {}
        returnValues["component_statistics"] = spreadsheet

    except Exception as exc:
        print("Exception encountered while running ConnectedComponents")
        print(exc)

    return returnValues

def transform_scalars(dataset):
    """This filter performs semi-automatic multithresholding of a data set.
    Voxels are automatically classified into a chosen number of classes such
    that inter-class variance of the voxel values is minimized. The output is a
    label map with one label per voxel class.
    """

    try:
        import itk
        import vtk
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s) itk, vtk, or tomviz.utils")
        print(exc)

    #----USER SPECIFIED VARIABLES----#
    ###NUMBEROFTHRESHOLDS### # Specify number of thresholds between classes
    ###ENABLEVALLEYEMPHASIS### # Enable valley emphasis.

    # Return values
    returnValues = None

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image
        itk_image = utils.convert_vtk_to_itk_image(dataset)
        itk_input_image_type = type(itk_image)

        # OtsuMultipleThresholdsImageFilter's wrapping requires that the input
        # and output image types be the same.
        # TODO - handle casting of float image types to some sensible integer
        # format.
        itk_threshold_image_type = itk_input_image_type

        # Otsu multiple threshold filter
        otsu_filter = itk.OtsuMultipleThresholdsImageFilter[
            itk_input_image_type, itk_threshold_image_type].New()
        otsu_filter.SetNumberOfThresholds(number_of_thresholds)
        otsu_filter.SetValleyEmphasis(enable_valley_emphasis)
        otsu_filter.SetInput(itk_image)
        otsu_filter.Update()

        print("Otsu threshold(s): %s" % (otsu_filter.GetThresholds(),))

        itk_image_data = otsu_filter.GetOutput()
        label_buffer = itk.PyBuffer[itk_threshold_image_type] \
            .GetArrayFromImage(itk_image_data)

        label_map_data_set = vtk.vtkImageData()
        label_map_data_set.CopyStructure(dataset)
        utils.set_label_map(label_map_data_set, label_buffer)

        # Set up dictionary to return operator results
        returnValues = {}
        returnValues["label_map"] = label_map_data_set

    except Exception as exc:
        print("Exception encountered while running OtsuMultipleThreshold")
        print(exc)

    return returnValues

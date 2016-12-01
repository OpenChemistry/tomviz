def transform_scalars(dataset, lower_threshold=40.0, upper_threshold=255.0):
    """This filter computes a binary threshold on the data set and
    stores the result in a child data set. It does not modify the dataset
    passed in."""

    returnValue = None

    # Try imports to make sure we have everything that is needed
    try:
        import itk
        import vtk
        from tomviz import itkutils
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image
        itk_image = itkutils.convert_vtk_to_itk_image(dataset)
        itk_input_image_type = type(itk_image)

        # We change the output type to unsigned char 3D
        # (itk.Image.UC3D) to save memory in the output label map
        # representation.
        itk_output_image_type = itk.Image.UC3

        # ITK's BinaryThresholdImageFilter does the hard work
        threshold_filter = itk.BinaryThresholdImageFilter[
            itk_input_image_type, itk_output_image_type].New()
        python_cast = itkutils.get_python_voxel_type(itk_image)
        threshold_filter.SetLowerThreshold(python_cast(lower_threshold))
        threshold_filter.SetUpperThreshold(python_cast(upper_threshold))
        threshold_filter.SetInsideValue(1)
        threshold_filter.SetOutsideValue(0)
        threshold_filter.SetInput(itk_image)
        threshold_filter.Update()

        # Set the output as a new child data object of the current data set
        label_map_data_set = vtk.vtkImageData()
        label_map_data_set.CopyStructure(dataset)

        itk_image_data = threshold_filter.GetOutput()
        label_buffer = itk.PyBuffer[
            itk_output_image_type].GetArrayFromImage(itk_image_data)
        utils.set_array(label_map_data_set, label_buffer)
        returnValue = {
            "thresholded_segmentation": label_map_data_set
        }

    except Exception as exc:
        print("Exception encountered while running BinaryThreshold")
        print(exc)

    return returnValue

def transform_scalars(dataset):
    """This filter computes a binary threshold on the data set and
    stores the result in a label map in the data set."""

    # Try imports to make sure we have everything that is needed
    try:
        import itk
        import vtk
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    # Set some filter parameters
    #----USER SPECIFIED VARIABLES-----#
    ###LOWERTHRESHOLD### # Specify lower threshold
    ###UPPERTHRESHOLD### # Specify upper threshold

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image
        itk_image = utils.convert_vtk_to_itk_image(dataset)
        itk_input_image_type = type(itk_image)

        # We change the output type to unsigned char 3D
        # (itk.Image.UC3D) to save memory in the output label map
        # representation.
        itk_output_image_type = itk.Image.UC3

        # ITK's BinaryThresholdImageFilter does the hard work
        threshold_filter = itk.BinaryThresholdImageFilter[itk_input_image_type,itk_output_image_type].New()
        threshold_filter.SetLowerThreshold(lower_threshold)
        threshold_filter.SetUpperThreshold(upper_threshold)
        threshold_filter.SetInsideValue(1)
        threshold_filter.SetOutsideValue(0)
        threshold_filter.SetInput(itk_image)
        threshold_filter.Update()

        utils.add_vtk_array_from_itk_image(threshold_filter.GetOutput(), dataset, 'LabelMap')

    except Exception as exc:
        print("Exception encountered while running BinaryThreshold");
        print(exc);

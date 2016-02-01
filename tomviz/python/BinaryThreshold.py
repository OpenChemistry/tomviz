def transform_scalars(dataset):
    """This filter computes a binary threshold on the data set and
    stores the result in a label map in the data set."""

    # Try imports to make sure we have everything that is needed
    try:
        import itk
        import vtk
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    # Set some filter parameters
    lower_threshold = 40
    upper_threshold = 255

    # Add a try/catch around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.

    try:
        # Set up some ITK variables
        itk_input_image_type = itk.Image.UC3
        itk_import = itk.VTKImageToImageFilter[itk_input_image_type].New()
        itk_import.SetInput(dataset)
        itk_import.Update()

        # Read the image into ITK
        itk_image = itk_import.GetOutput()

        # We change the output type to unsigned char 3D (itk.Image.UC3D) to save memory
        # in the output label map representation
        itk_output_image_type = itk.Image.UC3

        # ITK's BinaryThresholdImageFilter
        threshold_filter = itk.BinaryThresholdImageFilter[itk_input_image_type,itk_output_image_type].New()
        threshold_filter.SetLowerThreshold(lower_threshold)
        threshold_filter.SetUpperThreshold(upper_threshold)
        threshold_filter.SetInsideValue(1)
        threshold_filter.SetOutsideValue(0)
        threshold_filter.SetInput(itk_image)
        threshold_filter.Update()

        # Export the ITK image to a VTK image. No copying should take place.
        export_filter = itk.ImageToVTKImageFilter[itk_output_image_type].New()
        export_filter.SetInput(threshold_filter.GetOutput())
        export_filter.Update()

        # Get scalars from the temporary image and copy them to the data set
        result_image = export_filter.GetOutput()
        filter_array = result_image.GetPointData().GetArray(0)

        # Make a new instance of the array that will stick around after this
        # filters in this script are garbage collected
        label_map = filter_array.NewInstance()
        label_map.DeepCopy(filter_array) # Should be able to shallow copy?
        label_map.SetName('LabelMap')

        # Set a new point data array in the dataset
        dataset.GetPointData().AddArray(label_map)

    except Exception as exc:
        print("Exception encountered while running BinaryThreshold");
        print(exc);

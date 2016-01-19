def transform_scalars(dataset):
    """This filter computes a binary threshold on the data set.
    The output is a binary image marking the regions where voxels
    fall within a defined threshold."""

    # Try imports to make sure we have everything that is needed
    try:
        from tomviz import utils
        import numpy as np
        import itk
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    # Set some filter parameters
    lower_threshold = 40
    upper_threshold = 255

    # Get the current volume as a numpy array.
    array = utils.get_array(dataset)

    # Add a try/catch around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.

    try:
        # Set up some ITK variables
        itk_input_image_type = itk.Image.F3
        itk_converter = itk.PyBuffer[itk_input_image_type]

        # Read the image into ITK
        itk_image = itk_converter.GetImageFromArray(array)

        # We change the output type to unsigned char 3D (itk.Image.UC3D) to save memory
        # in the output label map representation
        itk_output_image_type = itk.Image.UC3

        # ITK's BinaryThresholdImageFilter
        filter = itk.BinaryThresholdImageFilter[itk_input_image_type,itk_output_image_type].New()
        filter.SetLowerThreshold(lower_threshold)
        filter.SetUpperThreshold(upper_threshold)
        filter.SetInput(itk_image)
        filter.Update()

        # Get the image back from ITK (result is a numpy image)
        result = itk.PyBuffer[itk_output_image_type].GetArrayFromImage(filter.GetOutput())

        # This is where the transformed data is set, it will display in tomviz.
        # TODO - set it as the label map array
        utils.set_array(dataset, result)

    except Exception as exc:
        print("Exception encountered while running BinaryThreshold");
        print(exc);

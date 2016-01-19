def transform_scalars(dataset):
    """This filter labels the connected components in a binary input.
    Usually this filter would follow a BinaryThreshold filter that produces
    a binary label map."""

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
    background_value = 0

    # Get the current volume as a numpy array.
    array = utils.get_array(dataset)

    # Add a try/catch around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.

    try:
        # Set up some ITK variables
        itk_input_image_type = itk.Image.US3
        itk_converter = itk.PyBuffer[itk_input_image_type]

        # Read the image into ITK
        itk_image = itk_converter.GetImageFromArray(array)

        # We need to set this to an integral type with a large maximum
        # value to acommodate potentially many connected components.
        # Unfortunately, the input pixel type limits this, so the
        # output from the threshold filter needs to be larger than
        # necessary.
        itk_threshold_image_type = itk.Image.US3

        # Binary threhsold filter
        thresholdFilter = itk.BinaryThresholdImageFilter[itk_input_image_type, itk_threshold_image_type].New();
        thresholdFilter.SetLowerThreshold(lower_threshold)
        thresholdFilter.SetUpperThreshold(upper_threshold)
        thresholdFilter.SetInput(itk_image)

        # Set output image type to unsigned long. There may be many connected components,
        # and an exception will be thrown if a label value is larger than the largest
        # representable value in the input pixel type.
        itk_output_image_type = itk.Image.US3

        # ConnectedComponentImageFilter
        connectedFilter = itk.ConnectedComponentImageFilter[itk_threshold_image_type,itk_output_image_type].New()
        connectedFilter.SetBackgroundValue(background_value)
        connectedFilter.SetInput(thresholdFilter.GetOutput())

        # Relabel filter. This will compress the label numbers to a
        # continugous range between 1 and n where n is the number of
        # labels. It will also sort the components from largest to
        # smallest, where the largest component has label 1, the
        # second largest has label 2, and so on...
        relabelFilter = itk.RelabelComponentImageFilter[itk_output_image_type, itk_output_image_type].New()
        relabelFilter.SetInput(connectedFilter.GetOutput())
        relabelFilter.SortByObjectSizeOn()
        relabelFilter.Update()

        # Get the image back from ITK (result is a numpy image)
        result = itk.PyBuffer[itk_output_image_type].GetArrayFromImage(relabelFilter.GetOutput())

        # This is where the transformed data is set, it will display in tomviz.
        #utils.set_array(dataset, result)
        utils.set_label_map(dataset, result)

    except Exception as exc:
        print("Exception encountered while running ConnectedComponents")
        print(exc)

def transform_scalars(dataset):
    """Define this method for Python operators that
    transform the input array."""
    from tomviz import utils
    import numpy as np
    import itk

    # Get the current volume as a numpy array.
    array = utils.get_array(dataset)

    # Set up some ITK variables
    itk_image_type = itk.Image.F3
    itk_converter = itk.PyBuffer[itk_image_type]

    # Read the image into ITK
    itk_image = itk_converter.GetImageFromArray(array)

    # ITK filter (I have no idea if this is right)
    filter = \
        itk.ConfidenceConnectedImageFilter[itk_image_type,itk.Image.SS3].New()
    filter.SetInitialNeighborhoodRadius(3)
    filter.SetMultiplier(3)
    filter.SetNumberOfIterations(25)
    filter.SetReplaceValue(255)
    filter.SetSeed((24,65,37))
    filter.SetInput(itk_image)
    filter.Update()

    # Get the image back from ITK (result is a numpy image)
    result = itk.PyBuffer[itk.Image.SS3].GetArrayFromImage(filter.GetOutput())

    # This is where the transformed data is set, it will display in tomviz.
    utils.set_array(dataset, result)

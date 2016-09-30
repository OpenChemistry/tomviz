def transform_scalars(dataset):
    """Dilate segmented objects with a given label by a cubic structuring element
    with a given radius.
    """

    try:
        import itk
        import vtk
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s) itk and vtk")
        print(exc)

    #----USER SPECIFIED VARIABLES-----#
    ###radius### # Specify the radius of the cube structuring element
    ###object_label### # Specify the label designating the segmented objects
    print("radius %d" % radius)
    print("object_label %d" % object_label)

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image
        itk_image = utils.convert_vtk_to_itk_image(dataset)
        itk_input_image_type = type(itk_image)

        itk_kernel_type = itk.FlatStructuringElement[3]
        itk_kernel = itk_kernel_type.Box(radius)

        dilate_filter = itk.BinaryDilateImageFilter[itk_input_image_type,
                                                    itk_input_image_type,
                                                    itk_kernel_type].New()
        dilate_filter.SetDilateValue(object_label)
        dilate_filter.SetKernel(itk_kernel)
        dilate_filter.SetInput(itk_image)
        dilate_filter.Update()
        itk_image_data = dilate_filter.GetOutput()

        label_buffer = itk.PyBuffer[itk_input_image_type].GetArrayFromImage(itk_image_data)

        utils.set_label_map(dataset, label_buffer)
    except Exception as exc:
        print("Exception encountered while running BinaryDilate")
        print(exc)

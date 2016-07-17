def transform_scalars(dataset):
    """This filter thresholds an image input, marking voxels within a
    lower and upper intensity range provided as foreground and the
    remaining as background, then generates a label map from the
    connected components in the foreground.
    """

    try:
        import itk
        import vtk
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s) itk and vtk")
        print(exc)

    #----USER SPECIFIED VARIABLES-----#
    ###LOWERTHRESHOLD### # Specify lower threshold
    ###UPPERTHRESHOLD### # Specify upper threshold
    background_value = 0

    # Add a try/except around the ITK portion. ITK exceptions are
    # passed up to the Python layer, so we can at least report what
    # went wrong with the script, e.g,, unsupported image type.
    try:
        # Get the ITK image
        itk_image = utils.convert_vtk_to_itk_image(dataset)
        itk_input_image_type = type(itk_image)

        # Set the output type for the binary threshold filter to
        # unsigned short (US). The maximum number representable by the
        # input pixel type in the connected components filter below
        # determines how many connected components can be extracted
        # from the binary image - if the type's maximum value is less
        # than the extracted components, the filter will throw an
        # exception.
        itk_threshold_image_type = itk.Image.US3

        # Cast the image to unsigned chars. Probably want floats at
        # some point, but this is known to work for this sequence of
        # operations.
        cast_filter = itk.CastImageFilter[itk_input_image_type, itk_threshold_image_type].New()
        cast_filter.SetInput(itk_image)

        # Binary threshold filter
        threshold_filter = itk.BinaryThresholdImageFilter[itk_threshold_image_type, itk_threshold_image_type].New()
        threshold_filter.SetLowerThreshold(lower_threshold)
        threshold_filter.SetUpperThreshold(upper_threshold)
        threshold_filter.SetInput(cast_filter.GetOutput())

        # We'll make the output image type for the connected
        # components filter be the same as that of the threshold
        # output image.
        itk_output_image_type = itk_threshold_image_type

        # ConnectedComponentImageFilter
        connected_filter = itk.ConnectedComponentImageFilter[itk_threshold_image_type,itk_output_image_type].New()
        connected_filter.SetBackgroundValue(background_value)
        connected_filter.SetInput(threshold_filter.GetOutput())

        # Relabel filter. This will compress the label numbers to a
        # continugous range between 1 and n where n is the number of
        # labels. It will also sort the components from largest to
        # smallest, where the largest component has label 1, the
        # second largest has label 2, and so on...
        relabel_filter = itk.RelabelComponentImageFilter[itk_output_image_type, itk_output_image_type].New()
        relabel_filter.SetInput(connected_filter.GetOutput())
        relabel_filter.SortByObjectSizeOn()
        relabel_filter.Update()

        utils.add_vtk_array_from_itk_image(relabel_filter.GetOutput(), dataset, 'LabelMap')

        # Now take the connected components results and compute things like volume
        # and surface area.
        shape_filter = itk.LabelImageToShapeLabelMapFilter.IUS3LM3.New()
        shape_filter.SetInput(relabel_filter.GetOutput())
        shape_filter.Update()

        # Set up arrays to hold the shape attribute data
        label_map = shape_filter.GetOutput()
        num_label_objects = label_map.GetNumberOfLabelObjects()

        # Convenience function for creating shape attribute arrays
        def create_attribute_array(name):
            array = vtk.vtkDoubleArray()
            array.SetName(name)
            array.SetNumberOfComponents(1)
            array.SetNumberOfTuples(num_label_objects)
            return array

        area_array = create_attribute_array('SurfaceArea')
        volume_array = create_attribute_array('Volume')
        surface_volume_ratio_array = create_attribute_array('SurfaceAreaToVolumeRatio')

        for i in xrange(0, num_label_objects):
            label_object = label_map.GetNthLabelObject(i)
            surface_area = label_object.GetPerimeter()
            area_array.InsertValue(i, surface_area)
            volume = label_object.GetPhysicalSize()
            volume_array.InsertValue(i, volume)
            surface_volume_ratio_array.InsertValue(i, surface_area / volume)

        # Add arrays as field data for now. Should probably produce a table
        # output in the future when we figure out how to support that. Field
        # data should appear basically the same as a table in the spreadsheet
        # view, and has the advantage that you do not need to pick a different
        # pipeline output, as you would if the arrays were stored in a vtkTable
        # output.
        field_data = dataset.GetFieldData()
        field_data.AddArray(area_array)
        field_data.AddArray(volume_array)
        field_data.AddArray(surface_volume_ratio_array)

    except Exception as exc:
        print("Exception encountered while running ConnectedComponents")
        print(exc)

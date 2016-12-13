def transform_scalars(dataset):
    """Computes certain attributes of labeled objects, including surface area,
    volume, surface-area-to-volume ratio, and centroid.
    """

    try:
        import itk
        import vtk
        from tomviz import itkutils
        from tomviz import utils
    except Exception as exc:
        raise exc

    returnValues = None

    scalarType = dataset.GetScalarType()
    if scalarType == vtk.VTK_FLOAT or scalarType == vtk.VTK_DOUBLE:
        raise Exception(
            "Label Object Attributes works only on images with integral types.")

    try:        
        # Set up arrays to hold the shape attribute data
        shape_label_map = itkutils.get_label_object_attributes(dataset)

        num_label_objects = shape_label_map.GetNumberOfLabelObjects()

        column_names = ['SurfaceArea', 'Volume', 'SurfaceAreaToVolumeRatio']
        import numpy as np
        # num_label_objects rows, 3 columns
        table = np.zeros((num_label_objects, len(column_names)))

        for i in xrange(0, num_label_objects):
            label_object = shape_label_map.GetNthLabelObject(i)
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
        print("Exception encountered while running Label Object Attributes")
        print(exc)
        raise(exc)

    return returnValues

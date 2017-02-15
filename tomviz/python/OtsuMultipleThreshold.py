import tomviz.operators


class OtsuMultipleThreshold(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, number_of_thresholds=1,
                          enable_valley_emphasis=False):
        """This filter performs semi-automatic multithresholding of a data set.
        Voxels are automatically classified into a chosen number of classes such
        that inter-class variance of the voxel values is minimized. The output
        is a label map with one label per voxel class.
        """

        try:
            import itk
            import itkExtras
            import itkTypes
            import vtk
            from tomviz import itkutils
            from tomviz import utils
        except Exception as exc:
            print("Could not import necessary module(s)")
            print(exc)

        # Return values
        returnValues = None

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g,, unsupported image type.
        try:
            # Get the ITK image
            itk_image = itkutils.convert_vtk_to_itk_image(dataset)
            itk_input_image_type = type(itk_image)

            # OtsuMultipleThresholdsImageFilter's wrapping requires that the
            # input and output image types be the same.
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

            # Cast threshold output to an integral type if needed.
            py_buffer_type = itk_threshold_image_type
            voxel_type = itkExtras.template(itk_threshold_image_type)[1][0]
            if voxel_type is itkTypes.F or voxel_type is itkTypes.D:
                # Unsigned char supports 256 labels, or 255 threshold levels.
                # This should be sufficient for all but the most unusual use
                # cases.
                py_buffer_type = itk.Image.UC3
                caster = itk.CastImageFilter[itk_threshold_image_type,
                                             py_buffer_type].New()
                caster.SetInput(itk_image_data)
                caster.Update()
                itk_image_data = caster.GetOutput()

            label_buffer = itk.PyBuffer[py_buffer_type] \
                .GetArrayFromImage(itk_image_data)

            label_map_dataset = vtk.vtkImageData()
            label_map_dataset.CopyStructure(dataset)
            utils.set_array(label_map_dataset, label_buffer)

            # Set up dictionary to return operator results
            returnValues = {}
            returnValues["label_map"] = label_map_dataset

        except Exception as exc:
            print("Exception encountered while running OtsuMultipleThreshold")
            print(exc)
            raise exc

        return returnValues

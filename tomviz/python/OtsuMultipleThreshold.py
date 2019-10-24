import tomviz.operators


class OtsuMultipleThreshold(tomviz.operators.CancelableOperator):

    def transform(self, dataset, number_of_thresholds=1,
                  enable_valley_emphasis=False):
        """This filter performs semi-automatic multithresholding of a data set.
        Voxels are automatically classified into a chosen number of classes such
        that inter-class variance of the voxel values is minimized. The output
        is a label map with one label per voxel class.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [10, 20, 70, 90, 100]

        try:
            import itk
            import itkExtras
            import itkTypes
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        # Return values
        returnValues = None

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g,, unsupported image type.
        try:
            self.progress.value = STEP_PCT[0]
            self.progress.message = "Converting data to ITK image"

            # Get the ITK image
            itk_image = itkutils.dataset_to_itk_image(dataset)
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
            itkutils.observe_filter_progress(self, otsu_filter,
                                             STEP_PCT[1], STEP_PCT[2])

            try:
                otsu_filter.Update()
            except RuntimeError:
                return

            print("Otsu threshold(s): %s" % (otsu_filter.GetThresholds(),))

            itk_image_data = otsu_filter.GetOutput()

            # Cast threshold output to an integral type if needed.
            py_buffer_type = itk_threshold_image_type
            voxel_type = itkExtras.template(itk_threshold_image_type)[1][0]
            if voxel_type is itkTypes.F or voxel_type is itkTypes.D:
                self.progress.message = "Casting output to integral type"

                # Unsigned char supports 256 labels, or 255 threshold levels.
                # This should be sufficient for all but the most unusual use
                # cases.
                py_buffer_type = itk.Image.UC3
                caster = itk.CastImageFilter[itk_threshold_image_type,
                                             py_buffer_type].New()
                caster.SetInput(itk_image_data)
                itkutils.observe_filter_progress(self, caster,
                                                 STEP_PCT[2], STEP_PCT[3])

                try:
                    caster.Update()
                except RuntimeError:
                    return

                itk_image_data = caster.GetOutput()

            self.progress.value = STEP_PCT[3]
            self.progress.message = "Saving results"

            label_map_dataset = dataset.create_child_dataset()
            itkutils.set_itk_image_on_dataset(itk_image_data, label_map_dataset,
                                              dtype=py_buffer_type)

            self.progress.value = STEP_PCT[4]

            # Set up dictionary to return operator results
            returnValues = {}
            returnValues["label_map"] = label_map_dataset

        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValues

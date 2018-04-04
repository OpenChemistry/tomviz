import tomviz.operators


class BinaryThreshold(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, lower_threshold=40.0,
                          upper_threshold=255.0):
        """This filter computes a binary threshold on the data set and
        stores the result in a child data set. It does not modify the dataset
        passed in."""

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [20, 40, 75, 90, 100]

        # Set up return value
        returnValue = None

        # Try imports to make sure we have everything that is needed
        try:
            self.progress.message = "Loading modules"
            import itk
            from vtkmodules.vtkCommonDataModel import vtkImageData
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g,, unsupported image type.
        try:
            self.progress.value = STEP_PCT[0]
            self.progress.message = "Converting data to ITK image"

            # Get the ITK image
            itk_image = itkutils.convert_vtk_to_itk_image(dataset)
            itk_input_image_type = type(itk_image)
            self.progress.value = STEP_PCT[1]
            self.progress.message = "Running filter"

            # We change the output type to unsigned char 3D
            # (itk.Image.UC3D) to save memory in the output label map
            # representation.
            itk_output_image_type = itk.Image.UC3

            # ITK's BinaryThresholdImageFilter does the hard work
            threshold_filter = itk.BinaryThresholdImageFilter[
                itk_input_image_type, itk_output_image_type].New()
            python_cast = itkutils.get_python_voxel_type(itk_image)
            threshold_filter.SetLowerThreshold(python_cast(lower_threshold))
            threshold_filter.SetUpperThreshold(python_cast(upper_threshold))
            threshold_filter.SetInsideValue(1)
            threshold_filter.SetOutsideValue(0)
            threshold_filter.SetInput(itk_image)
            itkutils.observe_filter_progress(self, threshold_filter,
                                             STEP_PCT[2], STEP_PCT[3])

            try:
                threshold_filter.Update()
            except RuntimeError:
                return returnValue

            from tomviz import utils
            self.progress.message = "Creating child data set"

            # Set the output as a new child data object of the current data set
            label_map_dataset = vtkImageData()
            label_map_dataset.CopyStructure(dataset)

            itkutils.set_array_from_itk_image(label_map_dataset,
                                              threshold_filter.GetOutput())
            self.progress.value = STEP_PCT[4]

            returnValue = {
                "thresholded_segmentation": label_map_dataset
            }

        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValue

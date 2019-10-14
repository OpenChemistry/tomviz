import tomviz.operators


class DefaultITKTransform(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        """Define this method for Python operators that transform the input
        array. This example uses an ITK filter to add 10 to each voxel value."""

        # Try imports to make sure we have everything that is needed
        try:
            from tomviz import itkutils
            import itk
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        self.progress.value = 0
        self.progress.maximum = 100

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g., unsupported image type.
        try:
            self.progress.value = 0
            self.progress.message = "Converting data to ITK image"

            # Get the ITK image
            itk_image = itk.GetImageViewFromArray(dataset.active_scalars)
            itk_input_image_type = type(itk_image)
            self.progress.value = 30
            self.progress.message = "Running filter"

            # ITK filter
            filter = itk.AddImageFilter[itk_input_image_type, # Input 1
                                        itk_input_image_type, # Input 2
                                        itk_input_image_type].New() # Output
            filter.SetInput1(itk_image)
            filter.SetConstant2(10)
            itkutils.observe_filter_progress(self, filter, 30, 70)

            try:
                filter.Update()
            except RuntimeError: # Exception thrown when ITK filter is aborted
                return

            self.progress.message = "Saving results"

            result = itk.GetArrayFromImage(filter.GetOutput())
            # Transpose the data to Fortran indexing
            dataset.active_scalars = result.transpose([2, 1, 0])

            self.progress.value = 100
        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

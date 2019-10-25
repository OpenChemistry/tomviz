import tomviz.operators


class BinaryMinMaxCurvatureFlow(tomviz.operators.CancelableOperator):

    def transform(self, dataset, stencil_radius=2, iterations=10,
                  threshold=50.0):
        """This filter smooths a binary image by evolving a level set with a
        curvature-based speed function. The Stencil Radius determines the scale
        of the noise to remove. The Threshold determines the iso-contour
        brightness to discriminate between two pixel classes.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [10, 20, 30, 70, 80, 90, 100]

        try:
            import itk
            import itkTypes
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        # Return values
        returnValue = None

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g,, unsupported image type.
        try:
            self.progress.value = STEP_PCT[0]
            self.progress.message = "Converting data to ITK image"
            # Get the ITK image
            itk_image = itkutils.dataset_to_itk_image(dataset)
            itk_input_image_type = type(itk_image)
            self.progress.message = "Casting input to float type"
            itk_filter_image_type = itk.Image[itkTypes.F,
                                              itk_image.GetImageDimension()]
            caster = itk.CastImageFilter[itk_input_image_type,
                                         itk_filter_image_type].New()
            caster.SetInput(itk_image)
            itkutils.observe_filter_progress(self, caster,
                                             STEP_PCT[1], STEP_PCT[2])

            try:
                caster.Update()
            except RuntimeError:
                return

            self.progress.message = "Running filter"
            smoothing_filter = itk.BinaryMinMaxCurvatureFlowImageFilter[
                itk_filter_image_type, itk_filter_image_type].New()
            smoothing_filter.SetThreshold(threshold)
            smoothing_filter.SetStencilRadius(stencil_radius)
            smoothing_filter.SetNumberOfIterations(iterations)
            smoothing_filter.SetTimeStep(0.0625)
            smoothing_filter.SetInput(caster)
            itkutils.observe_filter_progress(self, smoothing_filter,
                                             STEP_PCT[2], STEP_PCT[3])

            try:
                smoothing_filter.Update()
            except RuntimeError:
                return

            itk_image_data = smoothing_filter.GetOutput()

            # Cast output to the input type
            self.progress.message = "Casting output to input type"

            caster = itk.CastImageFilter[itk_filter_image_type,
                                         itk_input_image_type].New()
            caster.SetInput(itk_image_data)
            itkutils.observe_filter_progress(self, caster,
                                             STEP_PCT[3], STEP_PCT[4])

            try:
                caster.Update()
            except RuntimeError:
                return

            itk_image_data = caster.GetOutput()

            self.progress.value = STEP_PCT[5]
            self.progress.message = "Saving results"

            itkutils.set_itk_image_on_dataset(itk_image_data, dataset)

            self.progress.value = STEP_PCT[6]

        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValue

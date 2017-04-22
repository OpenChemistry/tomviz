import tomviz.operators


class PeronaMalikAnisotropicDiffusion(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, conductance=1.0, iterations=100,
                          timestep=0.0625):
        """This filter performs anisotropic diffusion on an image using
        the classic Perona-Malik, gradient magnitude-based equation.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [10, 20, 30, 90, 100]

        try:
            import itk
            import itkTypes
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        try:
            self.progress.value = STEP_PCT[0]
            self.progress.message = "Converting data to ITK image"

            # Get the ITK image. The itk.GradientAnisotropicDiffusionImageFilter
            # is templated over float pixel types only, so explicitly request a
            # float ITK image type.
            itk_image = itkutils.convert_vtk_to_itk_image(dataset)
            itk_input_image_type = type(itk_image)
            dimension = itk_image.GetImageDimension()
            itk_image_type = itk.Image[itkTypes.F, dimension]

            self.progress.message = "Casting input to float type"

            caster = itk.CastImageFilter[itk_input_image_type,
                                         itk_image_type].New()
            caster.SetInput(itk_image)
            itkutils.observe_filter_progress(self, caster,
                                             STEP_PCT[1], STEP_PCT[2])

            try:
                caster.Update()
            except RuntimeError:
                return

            self.progress.value = STEP_PCT[1]
            self.progress.message = "Running filter"

            DiffusionFilterType = \
                itk.GradientAnisotropicDiffusionImageFilter[itk_image_type,
                                                            itk_image_type]
            diffusion_filter = DiffusionFilterType.New()
            diffusion_filter.SetConductanceParameter(conductance)
            diffusion_filter.SetNumberOfIterations(iterations)
            diffusion_filter.SetTimeStep(timestep)
            diffusion_filter.SetInput(caster)
            itkutils.observe_filter_progress(self, diffusion_filter,
                                             STEP_PCT[2], STEP_PCT[3])

            try:
                diffusion_filter.Update()
            except RuntimeError:
                return

            self.progress.message = "Saving results"

            itkutils.set_array_from_itk_image(dataset,
                                              diffusion_filter.GetOutput())

            self.progress.value = STEP_PCT[4]
        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

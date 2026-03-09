import tomviz.operators


class PeronaMalikAnisotropicDiffusion(tomviz.operators.CancelableOperator):

    def transform(self, dataset, conductance=1.0, iterations=100,
                  timestep=0.0625):
        """This filter performs anisotropic diffusion on an image using
        the classic Perona-Malik, gradient magnitude-based equation.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [10, 20, 90, 100]

        try:
            import numpy as np
            import itk
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
            array = dataset.active_scalars.astype(np.float32)
            itk_image = itk.GetImageViewFromArray(array)
            itk_image.SetSpacing(dataset.spacing)
            itk_image_type = type(itk_image)

            self.progress.value = STEP_PCT[1]
            self.progress.message = "Running filter"

            DiffusionFilterType = \
                itk.GradientAnisotropicDiffusionImageFilter[itk_image_type,
                                                            itk_image_type]
            diffusion_filter = DiffusionFilterType.New()
            diffusion_filter.SetConductanceParameter(conductance)
            diffusion_filter.SetNumberOfIterations(iterations)
            diffusion_filter.SetTimeStep(timestep)
            diffusion_filter.SetInput(itk_image)
            itkutils.observe_filter_progress(self, diffusion_filter,
                                             STEP_PCT[1], STEP_PCT[2])

            try:
                diffusion_filter.Update()
            except RuntimeError:
                return

            self.progress.message = "Saving results"

            itkutils.set_itk_image_on_dataset(diffusion_filter.GetOutput(),
                                              dataset)

            self.progress.value = STEP_PCT[3]
        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

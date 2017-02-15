import tomviz.operators


class PeronaMalikAnisotropicDiffusion(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, conductance=1.0, iterations=100,
                          timestep=0.125):
        """This filter performs anisotropic diffusion on an image using
        the classic Perona-Malik, gradient magnitude-based equation.
        """

        try:
            import itk
            import itkTypes
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            print(exc)

        try:
            # Get the ITK image. The itk.GradientAnisotropicDiffusionImageFilter
            # is templated over float pixel types only, so explicitly request a
            # float ITK image type.
            itk_image = itkutils.convert_vtk_to_itk_image(dataset, itkTypes.F)
            itk_image_type = type(itk_image)

            DiffusionFilterType = \
                itk.GradientAnisotropicDiffusionImageFilter[itk_image_type,
                                                            itk_image_type]
            diffusion_filter = DiffusionFilterType.New()
            diffusion_filter.SetConductanceParameter(conductance)
            diffusion_filter.SetNumberOfIterations(iterations)
            diffusion_filter.SetTimeStep(timestep)
            diffusion_filter.SetInput(itk_image)
            diffusion_filter.Update()
            itkutils.set_array_from_itk_image(dataset,
                                              diffusion_filter.GetOutput())
        except Exception as exc:
            print("Exception encountered while running"
                  "PeronaMalikAnisotropicDiffusion")
            print(exc)

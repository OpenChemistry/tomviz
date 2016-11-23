def transform_scalars(dataset, conductance=1.0, iterations=100, timestep=0.125):
    """This filter performs anisotropic diffusion on an image using
    the classic Perona-Malik, gradient magnitude-based equation.
    """

    try:
        import itk
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s)")
        print(exc)

    try:
        # Get the ITK image. The itk.GradientAnisotropicDiffusionImageFilter
        # is templated over float pixel types only, so explicitly request a
        # float ITK image type.
        import itkTypes
        itk_image = utils.convert_vtk_to_itk_image(dataset, itkTypes.F)
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

        diffusion_image = diffusion_filter.GetOutput()

        PyBufferType = itk.PyBuffer[itk_image_type]
        new_scalars = PyBufferType.GetArrayFromImage(diffusion_image)
        utils.set_array(dataset, new_scalars)

    except Exception as exc:
        print("Exception encountered while running"
              "PeronaMalikAnisotropicDiffusion")
        print(exc)

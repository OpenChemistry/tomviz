def transform_scalars(dataset):
    """This filter performs anisotropic diffusion on an image using
    the classic Perona-Malik, gradient magnitude-based equation.
    """

    try:
        import itk
        from tomviz import utils
    except Exception as exc:
        print("Could not import necessary module(s) itk and vtk")
        print(exc)

    #----USER SPECIFIED VARIABLES-----#
    ###conductance### # Conductance parameter controlling sensitivity to edges.
    ###iterations### # Number of iterations to run
    ###timestep### # Time step used for solving the diffusion equation

    try:
        # Get the ITK image
        itk_image = utils.convert_vtk_to_itk_image(dataset)
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

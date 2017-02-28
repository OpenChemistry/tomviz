import tomviz.operators


class UnsharpMask(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, amount=0.5, threshold=0.0,
                          sigma=1.0):
        """This filter performs anisotropic diffusion on an image using
        the classic Perona-Malik, gradient magnitude-based equation.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        step_pct = iter([10, 20, 90, 100])

        try:
            import itk
            import itkTypes
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        try:
            self.progress.message = "Converting data to ITK image"
            self.progress.value = 0

            # Get the ITK image. The itk.GradientAnisotropicDiffusionImageFilter
            # is templated over float pixel types only, so explicitly request a
            # float ITK image type.
            itk_image = itkutils.convert_vtk_to_itk_image(dataset)
            self.progress.value = next(step_pct)
            itk_image_type = type(itk_image)


            self.progress.message = "Running filter"
            self.progress.value = next(step_pct)

            unsharp_mask = \
                itk.UnsharpMaskImageFilter.New(Input=itk_image)
            unsharp_mask.SetAmount(amount)
            unsharp_mask.SetThreshold(threshold)
            unsharp_mask.SetSigma(sigma)
            self.progress.value

            try:
                unsharp_mask.Update()
            except RuntimeError:
                return
            print('update succeeded')
            print(itk_image)
            print(unsharp_mask.GetOutput())

            self.progress.message = "Saving results"
            self.progress.value = next(step_pct)

            enhanced = unsharp_mask.GetOutput()
            itkutils.set_array_from_itk_image(dataset,
                                              enhanced)

            self.progress.value = next(step_pct)
            print(self.progress.value)
        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

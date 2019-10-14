import tomviz.operators


class UnsharpMask(tomviz.operators.CancelableOperator):

    def transform(self, dataset, amount=0.5, threshold=0.0, sigma=1.0):
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
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        try:
            self.progress.message = "Converting data to ITK image"
            self.progress.value = 0

            # Get the ITK image.
            itk_image = itk.GetImageViewFromArray(dataset.active_scalars)
            self.progress.value = next(step_pct)

            self.progress.message = "Running filter"
            self.progress.value = next(step_pct)

            unsharp_mask = \
                itk.UnsharpMaskImageFilter.New(Input=itk_image)
            unsharp_mask.SetAmount(amount)
            unsharp_mask.SetThreshold(threshold)
            unsharp_mask.SetSigma(sigma)
            itkutils.observe_filter_progress(self, unsharp_mask,
                                             self.progress.value,
                                             next(step_pct))

            try:
                unsharp_mask.Update()
            except RuntimeError:
                return

            self.progress.message = "Saving results"

            result = itk.GetArrayFromImage(unsharp_mask.GetOutput())
            # Transpose the data to Fortran indexing
            dataset.active_scalars = result.transpose([2, 1, 0])

            self.progress.value = next(step_pct)
        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

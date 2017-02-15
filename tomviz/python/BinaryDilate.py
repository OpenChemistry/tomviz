import tomviz.operators


class BinaryDilate(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, structuring_element_id=0, radius=1,
                          object_label=1, background_label=0):
        """Dilate segmented objects with a given label by a spherically symmetric
        structuring element with a given radius.
        """

        try:
            import itk
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            print(exc)

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g,, unsupported image type.
        try:
            # Get the ITK image
            itk_image = itkutils.convert_vtk_to_itk_image(dataset)
            itk_input_image_type = type(itk_image)

            itk_kernel_type = itk.FlatStructuringElement[3]
            if (structuring_element_id == 0):
                itk_kernel = itk_kernel_type.Box(radius)
            elif (structuring_element_id == 1):
                itk_kernel = itk_kernel_type.Ball(radius)
            elif (structuring_element_id == 2):
                itk_kernel = itk_kernel_type.Cross(radius)
            else:
                raise Exception('Invalid kernel shape id %d' %
                                structuring_element_id)

            dilate_filter = itk.BinaryDilateImageFilter[itk_input_image_type,
                                                        itk_input_image_type,
                                                        itk_kernel_type].New()
            dilate_filter.SetDilateValue(object_label)
            dilate_filter.SetBackgroundValue(background_label)
            dilate_filter.SetKernel(itk_kernel)
            dilate_filter.SetInput(itk_image)
            dilate_filter.Update()
            itkutils.set_array_from_itk_image(dataset,
                                              dilate_filter.GetOutput())
        except Exception as exc:
            print("Exception encountered while running BinaryDilate")
            print(exc)

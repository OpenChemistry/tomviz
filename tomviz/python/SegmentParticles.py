import tomviz.operators


def median_filter(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils

    median_filter = \
        itk.MedianImageFilter.New(Input=input_image)
    median_radius = itk.Size[3]()
    median_radius.Fill(1)
    median_filter.SetRadius(median_radius)

    operator.progress.message = "Median filter"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, median_filter,
                                     progress, progress + next(step_pct))
    try:
        median_filter.Update()
    except RuntimeError:
        return

    return median_filter.GetOutput()


def opening_by_reconstruction(operator, step_pct, input_image,
                              structuring_element):
    import itk
    from tomviz import itkutils

    opening = \
        itk.OpeningByReconstructionImageFilter.New(Input=input_image)
    opening.SetKernel(structuring_element)

    operator.progress.message = "Opening by reconstruction"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, opening,
                                     progress, progress + next(step_pct))

    try:
        opening.Update()
    except RuntimeError:
        return

    return opening.GetOutput()


def threshold(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils
    import itkExtras
    import itkTypes

    otsu_filter = \
        itk.OtsuMultipleThresholdsImageFilter.New(Input=input_image)
    otsu_filter.SetNumberOfThresholds(1)
    operator.progress.message = "Otsu threshold"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, otsu_filter,
                                     progress, progress + next(step_pct))

    try:
        otsu_filter.Update()
    except RuntimeError:
        return

    # Cast threshold output to an integral type if needed.
    input_image_type = type(input_image)
    voxel_type = itkExtras.template(input_image_type)[1][0]
    if voxel_type is itkTypes.F or voxel_type is itkTypes.D:
        operator.progress.message = "Casting output to integral type"

        # Unsigned char supports 256 labels, or 255 threshold levels.
        # This should be sufficient for all but the most unusual use
        # cases.
        py_buffer_type = itk.Image.UC3
        caster = itk.CastImageFilter[input_image_type,
                                     py_buffer_type].New()
        caster.SetInput(thresholded)
        progress = operator.progress.value
        itkutils.observe_filter_progress(operator, caster,
                                         progress, progress + next(step_pct))

        try:
            caster.Update()
        except RuntimeError:
            return

        thresholded = caster.GetOutput()

    thresholded = otsu_filter.GetOutput()
    return thresholded


def morphological_closing(operator, step_pct, input_image, structuring_element):
    import itk
    from tomviz import itkutils

    closer = \
        itk.GrayscaleMorphologicalClosingImageFilter.New(Input=input_image)
    closer.SetKernel(structuring_element)
    operator.progress.message = "Morphological closing"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, closer,
                                     progress, progress + next(step_pct))

    try:
        closer.Update()
    except RuntimeError:
        return

    return closer.GetOutput()


def morphological_opening(operator, step_pct, input_image, structuring_element):
    import itk
    from tomviz import itkutils

    opener = \
        itk.GrayscaleMorphologicalOpeningImageFilter.New(Input=input_image)
    opener.SetKernel(structuring_element)
    operator.progress.message = "Morphological opening"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, opener,
                                     progress, progress + next(step_pct))

    try:
        opener.Update()
    except RuntimeError:
        return

    return opener.GetOutput()


def fill_holes(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils

    fill_hole = \
        itk.GrayscaleFillholeImageFilter.New(Input=input_image)
    operator.progress.message = "Fill holes"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, fill_hole,
                                     progress, progress + next(step_pct))

    try:
        fill_hole.Update()
    except RuntimeError:
        return

    return fill_hole.GetOutput()


class SegmentParticles(tomviz.operators.CancelableOperator):

    def transform(self, dataset, minimum_radius=4):
        """Segment spherical particles from a homogeneous, dark background.
        Even if the particles have pores, they are segmented as solid
        structures.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        step_pct = iter([10, 10, 10, 10, 10, 10, 10, 10, 10, 10])

        try:
            import itk
            from tomviz import itkutils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        # Return values
        returnValues = None

        # Add a try/except around the ITK portion. ITK exceptions are
        # passed up to the Python layer, so we can at least report what
        # went wrong with the script, e.g,, unsupported image type.
        try:
            self.progress.message = "Converting data to ITK image"

            # Get the ITK image
            itk_input_image = itkutils.dataset_to_itk_image(dataset)
            self.progress.value = next(step_pct)

            smoothed = median_filter(self, step_pct, itk_input_image)

            Dimension = 3
            StructuringElementType = itk.FlatStructuringElement[Dimension]
            structuring_element = StructuringElementType.Ball(minimum_radius)

            # Reduces reconstruction streak artifact effects and artifacts far
            # from the center of the image.
            opened = opening_by_reconstruction(self, step_pct, smoothed,
                                               structuring_element)

            thresholded = threshold(self, step_pct, opened)

            # Removes structures smaller than the structuring element while
            # retaining particle shape
            # Grayscale implementation is faster than binary
            cleaned = opening_by_reconstruction(self, step_pct, thresholded,
                                                structuring_element)

            # Fill in pores
            # Grayscale implementation is faster than binary
            closed = morphological_closing(self, step_pct, cleaned,
                                           structuring_element)

            # Fill in pores
            # Grayscale implementation is faster than binary
            filled = fill_holes(self, step_pct, closed)

            # Disconnect separate particles and reduce reconstruction
            opening = morphological_opening(self, step_pct, filled,
                                            structuring_element)

            self.progress.message = "Saving results"

            label_map_dataset = dataset.create_child_dataset()
            itkutils.set_itk_image_on_dataset(opening, label_map_dataset,
                                              dtype=type(itk_input_image))

            # Set up dictionary to return operator results
            returnValues = {}
            returnValues["label_map"] = label_map_dataset

        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValues

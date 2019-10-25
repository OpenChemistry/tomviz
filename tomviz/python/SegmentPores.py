import tomviz.operators


def median_filter(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils

    median_filter = \
        itk.MedianImageFilter.New(Input=input_image)
    dimension = input_image.GetImageDimension()
    median_radius = itk.Size[dimension]()
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


def unsharp_mask(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils
    import numpy as np

    enhancer = \
        itk.UnsharpMaskImageFilter.New(Input=input_image)
    enhancer.SetAmount(3.0)
    spacing = input_image.GetSpacing()
    enhancer.SetSigma(2.0 * np.max(spacing))

    operator.progress.message = "Unsharp mask"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, enhancer,
                                     progress, progress + next(step_pct))
    try:
        enhancer.Update()
    except RuntimeError:
        return

    return enhancer.GetOutput()


def get_distance(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils

    distance_mapper = \
        itk.SignedMaurerDistanceMapImageFilter.New(Input=input_image)
    distance_mapper.SetInsideIsPositive(True)
    distance_mapper.SetSquaredDistance(False)
    distance_mapper.SetUseImageSpacing(True)

    operator.progress.message = "Distance map"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, distance_mapper,
                                     progress, progress + next(step_pct))
    try:
        distance_mapper.Update()
    except RuntimeError:
        return

    return distance_mapper.GetOutput()


def invert(operator, step_pct, input_image):
    import itk
    from tomviz import itkutils

    inverter = \
        itk.NotImageFilter.New(Input=input_image)

    operator.progress.message = "Invert  threshold"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, inverter,
                                     progress, progress + next(step_pct))
    try:
        inverter.Update()
    except RuntimeError:
        return

    return inverter.GetOutput()


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
        dimension = input_image_type.GetImageDimension()
        py_buffer_type = itk.Image[itk.UC, dimension]
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


def watershed(operator, step_pct, input_image, level):
    import itk
    from tomviz import itkutils

    dimension = input_image.GetImageDimension()
    watershed_input_type = itk.Image[itk.F, dimension]
    watershed_output_type = itk.Image[itk.US, dimension]
    caster = itk.CastImageFilter[type(input_image), watershed_input_type].New()
    caster.SetInput(input_image)

    watershed_filter = \
        itk.MorphologicalWatershedImageFilter[
            watershed_input_type,
            watershed_output_type].New(Input=caster.GetOutput())
    watershed_filter.SetLevel(level)
    watershed_filter.SetMarkWatershedLine(True)
    watershed_filter.SetFullyConnected(True)

    operator.progress.message = "Watershed filter"
    progress = operator.progress.value
    itkutils.observe_filter_progress(operator, watershed_filter,
                                     progress, progress + next(step_pct))
    try:
        watershed_filter.Update()
    except RuntimeError:
        return

    return watershed_filter.GetOutput()


def apply_mask(operator, step_pct, input_image1, input_image2):
    import itk

    dimension = input_image1.GetImageDimension()
    output_type = itk.Image[itk.US, dimension]
    caster1 = itk.CastImageFilter[type(input_image1), output_type].New()
    caster1.SetInput(input_image1)
    caster2 = itk.CastImageFilter[type(input_image2), output_type].New()
    caster2.SetInput(input_image2)
    mask_filter = \
        itk.MultiplyImageFilter.New(Input1=caster1.GetOutput(),
                                    Input2=caster2.GetOutput())

    operator.progress.message = "Apply mask"
    operator.progress.value += next(step_pct)
    try:
        mask_filter.Update()
    except RuntimeError:
        return

    return mask_filter.GetOutput()


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


def encapsulate(operator, step_pct, input_image, mask, structuring_element):
    import itk
    from tomviz import itkutils

    dilater = \
        itk.GrayscaleDilateImageFilter.New(Input=input_image)
    dilater.SetKernel(structuring_element)
    operator.progress.message = "Encapsulate"
    progress = operator.progress.value
    nextprogress = progress + next(step_pct)
    itkutils.observe_filter_progress(operator, dilater,
                                     progress, nextprogress)

    xor_filter = \
        itk.XorImageFilter.New(Input1=mask, Input2=dilater.GetOutput())
    progress = nextprogress
    nextprogress = progress + next(step_pct)
    itkutils.observe_filter_progress(operator, xor_filter,
                                     progress, nextprogress)

    or_filter = \
        itk.OrImageFilter.New(Input1=input_image, Input2=xor_filter.GetOutput())
    progress = nextprogress
    nextprogress = progress + next(step_pct)
    itkutils.observe_filter_progress(operator, or_filter,
                                     progress, nextprogress)

    try:
        or_filter.Update()
    except RuntimeError:
        return

    return or_filter.GetOutput()


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


class SegmentPores(tomviz.operators.CancelableOperator):

    def transform(self, dataset, minimum_radius=0.5, maximum_radius=6.):
        """Segment pores. The pore size must be greater than the minimum radius
        and less than the maximum radius.  Pores will be separated according to
        the minimum radius."""

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        step_pct = iter([5, 5, 5, 5, 5, 5, 5, 5, 5, 30, 10, 5, 5, 5])

        try:
            import itk
            from tomviz import itkutils
            import numpy as np
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

            # Reduce noise
            smoothed = median_filter(self, step_pct, itk_input_image)

            # Enhance pore contrast
            enhanced = unsharp_mask(self, step_pct, smoothed)

            thresholded = threshold(self, step_pct, enhanced)

            dimension = itk_input_image.GetImageDimension()
            spacing = itk_input_image.GetSpacing()
            closing_radius = itk.Size[dimension]()
            closing_radius.Fill(1)
            for dim in range(dimension):
                radius = int(np.round(maximum_radius / spacing[dim]))
                if radius > closing_radius[dim]:
                    closing_radius[dim] = radius
            StructuringElementType = itk.FlatStructuringElement[dimension]
            structuring_element = \
                StructuringElementType.Ball(closing_radius)
            particle_mask = morphological_closing(self, step_pct, thresholded,
                                                  structuring_element)

            encapsulated = encapsulate(self, step_pct, thresholded,
                                       particle_mask, structuring_element)

            distance = get_distance(self, step_pct, encapsulated)

            segmented = watershed(self, step_pct, distance, minimum_radius)

            inverted = invert(self, step_pct, thresholded)

            segmented.DisconnectPipeline()
            inverted.DisconnectPipeline()
            separated = apply_mask(self, step_pct, segmented, inverted)

            separated.DisconnectPipeline()
            particle_mask.DisconnectPipeline()
            in_particles = apply_mask(self, step_pct, separated, particle_mask)

            opening_radius = itk.Size[dimension]()
            opening_radius.Fill(1)
            for dim in range(dimension):
                radius = int(np.round(minimum_radius / spacing[dim]))
                if radius > opening_radius[dim]:
                    opening_radius[dim] = radius
            structuring_element = \
                StructuringElementType.Ball(opening_radius)
            opened = opening_by_reconstruction(self, step_pct, in_particles,
                                               structuring_element)

            self.progress.message = "Saving results"

            label_map_dataset = dataset.create_child_dataset()
            itkutils.set_itk_image_on_dataset(opened, label_map_dataset)

            # Set up dictionary to return operator results
            returnValues = {}
            returnValues["label_map"] = label_map_dataset

        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValues

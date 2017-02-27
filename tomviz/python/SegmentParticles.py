import tomviz.operators


class SegmentParticles(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, minimum_radius=4):
        """Segment spherical particles from a homogeneous, dark background.
        Even if the particles have pores, they are segmented as solid
        structures.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [10, 20, 30, 40, 50, 60, 80, 90, 100]

        try:
            import itk
            import itkExtras
            import itkTypes
            import vtk
            from tomviz import itkutils
            from tomviz import utils
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
            itk_input_image = itkutils.convert_vtk_to_itk_image(dataset)
            itk_input_image_type = type(itk_input_image)
            self.progress.value = STEP_PCT[0]

            Dimension = 3

            median_filter = \
                itk.MedianImageFilter.New(Input=itk_input_image)
            median_radius = itk.Size[Dimension]()
            median_radius.Fill(1)
            median_filter.SetRadius(median_radius)

            self.progress.message = "Median filter"
            itkutils.observe_filter_progress(self, median_filter,
                                             STEP_PCT[0], STEP_PCT[1])
            try:
                median_filter.Update()
                self.progress.value = STEP_PCT[1]
            except RuntimeError:
                return

            StructuringElementType = itk.FlatStructuringElement[Dimension]
            structuring_element = StructuringElementType.Ball(minimum_radius)

            # Reduces reconstruction streak artifact effects and artifacts far
            # from the center of the image.
            opening = \
                itk.OpeningByReconstructionImageFilter[
                    itk_input_image_type,
                    itk_input_image_type,
                    StructuringElementType].New()
            opening.SetInput(itk_input_image)
            opening.SetKernel(structuring_element)

            self.progress.message = "Opening by reconstruction"
            itkutils.observe_filter_progress(self, opening,
                                             STEP_PCT[1], STEP_PCT[2])

            try:
                opening.Update()
                self.progress.value = STEP_PCT[2]
            except RuntimeError:
                return

            opened = opening.GetOutput()
            otsu_filter = \
                itk.OtsuMultipleThresholdsImageFilter.New(Input=opened)
            otsu_filter.SetNumberOfThresholds(1)
            self.progress.message = "Otsu threshold"
            itkutils.observe_filter_progress(self, otsu_filter,
                                             STEP_PCT[2], STEP_PCT[3])

            try:
                otsu_filter.Update()
                self.progress.value = STEP_PCT[3]
            except RuntimeError:
                return

            # Cast threshold output to an integral type if needed.
            py_buffer_type = itk_input_image_type
            voxel_type = itkExtras.template(itk_input_image_type)[1][0]
            if voxel_type is itkTypes.F or voxel_type is itkTypes.D:
                self.progress.message = "Casting output to integral type"

                # Unsigned char supports 256 labels, or 255 threshold levels.
                # This should be sufficient for all but the most unusual use
                # cases.
                py_buffer_type = itk.Image.UC3
                caster = itk.CastImageFilter[itk_input_image_type,
                                             py_buffer_type].New()
                caster.SetInput(thresholded)
                itkutils.observe_filter_progress(self, caster,
                                                 STEP_PCT[3], STEP_PCT[4])

                try:
                    caster.Update()
                    self.progress.value = STEP_PCT[4]
                except RuntimeError:
                    return

                thresholded = caster.GetOutput()

            thresholded = otsu_filter.GetOutput()
            # Removes structures smaller than the structuring element while
            # retaining particle shape
            # Grayscale implementation is faster than binary
            binary_opening_by_reconstruction = \
                itk.OpeningByReconstructionImageFilter.New(Input=thresholded)
            binary_opening_by_reconstruction.SetKernel(structuring_element)
            self.progress.message = "Binary opening by reconstruction"
            itkutils.observe_filter_progress(self,
                                             binary_opening_by_reconstruction,
                                             STEP_PCT[4], STEP_PCT[5])

            try:
                binary_opening_by_reconstruction.Update()
                self.progress.value = STEP_PCT[5]
            except RuntimeError:
                return

            cleaned = binary_opening_by_reconstruction.GetOutput()
            # Fill in pores
            # Grayscale implementation is faster than binary
            closer = \
                itk.GrayscaleMorphologicalClosingImageFilter.New(Input=cleaned)
            closing_structuring_element = \
                StructuringElementType.Ball(int(minimum_radius * 3. / 4.))
            closer.SetKernel(closing_structuring_element)
            self.progress.message = "Morphological closing"
            itkutils.observe_filter_progress(self, closer,
                                             STEP_PCT[5], STEP_PCT[6])

            try:
                closer.Update()
                self.progress.value = STEP_PCT[6]
            except RuntimeError:
                return

            closed = closer.GetOutput()
            # Fill in pores
            # Grayscale implementation is faster than binary
            fill_hole = \
                itk.GrayscaleFillholeImageFilter.New(Input=closed)
            self.progress.message = "Fill holes"
            itkutils.observe_filter_progress(self, fill_hole,
                                             STEP_PCT[6], STEP_PCT[7])

            try:
                fill_hole.Update()
                self.progress.value = STEP_PCT[7]
            except RuntimeError:
                return

            self.progress.message = "Saving results"

            label_buffer = itk.PyBuffer[py_buffer_type] \
                .GetArrayFromImage(fill_hole.GetOutput())

            label_map_dataset = vtk.vtkImageData()
            label_map_dataset.CopyStructure(dataset)
            utils.set_array(label_map_dataset, label_buffer, isFortran=False)

            self.progress.value = STEP_PCT[8]

            # Set up dictionary to return operator results
            returnValues = {}
            returnValues["label_map"] = label_map_dataset

        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValues

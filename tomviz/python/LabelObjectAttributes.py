import tomviz.operators


class LabelObjectAttributes(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        """Computes certain attributes of labeled objects, including surface
        area, volume, surface-area-to-volume ratio, and centroid.
        """

        # Initial progress
        self.progress.value = 0
        self.progress.maximum = 100

        # Approximate percentage of work completed after each step in the
        # transform
        STEP_PCT = [10, 20, 80, 90, 100]

        try:
            import numpy as np
            from tomviz import itkutils
            from tomviz import utils
        except Exception as exc:
            print("Could not import necessary module(s)")
            raise exc

        returnValues = None

        if np.issubdtype(dataset.active_scalars.dtype, np.floating):
            raise Exception(
                "Label Object Attributes works only on \
                 images with integral types.")

        try:
            self.progress.value = STEP_PCT[0]
            self.progress.message = "Computing label object attributes"

            # Set up arrays to hold the shape attribute data
            def progress_func(fraction):
                self.progress.value = \
                    int(fraction * (STEP_PCT[2] - STEP_PCT[1]) + STEP_PCT[1])
                return self.canceled

            shape_label_map = itkutils. \
                get_label_object_attributes(dataset, progress_func)
            if shape_label_map is None:
                return returnValues

            num_label_objects = shape_label_map.GetNumberOfLabelObjects()

            column_names = ['SurfaceArea', 'Volume', 'SurfaceAreaToVolumeRatio']
            import numpy as np
            # num_label_objects rows, 3 columns
            table = np.zeros((num_label_objects, len(column_names)))

            self.progress.message = "Computing attribute values"
            for i in range(0, num_label_objects):
                label_object = shape_label_map.GetNthLabelObject(i)
                surface_area = label_object.GetPerimeter()
                table[i, 0] = surface_area
                volume = label_object.GetPhysicalSize()
                table[i, 1] = volume
                table[i, 2] = surface_area / volume

            self.progress.value = STEP_PCT[3]
            self.progress.message = "Creating spreadsheet of results"

            # Create a spreadsheet data set from table data
            spreadsheet = utils.make_spreadsheet(column_names, table)

            # Set up dictionary to return operator results
            returnValues = {}
            returnValues["component_statistics"] = spreadsheet

            self.progress.value = STEP_PCT[4]
        except Exception as exc:
            print("Problem encountered while running %s" %
                  self.__class__.__name__)
            raise exc

        return returnValues

import tomviz.operators


class LabelObjectDistanceFromPrincipalAxis(tomviz.operators.CancelableOperator):

    def transform(self, dataset, label_value=1, principal_axis=0):
        """Computes the distance from the centroid of each connected component
        in the label object with the given label_value to the given principal
        axis and store that distance in each voxel of the label object connected
        component. A principal_axis of 0 is first principal axis, 1 is the
        second, and 2 is third.
        """

        import copy
        import numpy as np
        from tomviz import itkutils

        self.progress.maximum = 100
        self.progress.value = 0

        STEP_PCT = [20, 60, 80, 100]

        # These are obtained from the vtkDataObject
        axis = itkutils.get_principal_axes(dataset, principal_axis)
        center = itkutils.get_center(dataset)

        # Blank out the undesired label values
        data_shape = dataset.active_scalars.shape
        scalars = dataset.active_scalars.flatten(order='F')
        scalars[scalars != label_value] = 0
        dataset.active_scalars = scalars.reshape(data_shape, order='F')
        self.progress.value = STEP_PCT[0]

        # Get connected components of voxels labeled by label value
        def connected_progress_func(fraction):
            self.progress.value = \
                int(fraction * (STEP_PCT[1] - STEP_PCT[0]) + STEP_PCT[0])
            return self.canceled

        itkutils.connected_components(dataset, 0, connected_progress_func)

        # Get shape attributes
        def label_progress_func(fraction):
            self.progress.value = \
                int(fraction * (STEP_PCT[2] - STEP_PCT[1]) + STEP_PCT[1])
            return self.canceled

        shape_label_map = \
            itkutils.get_label_object_attributes(dataset, label_progress_func)
        num_label_objects = shape_label_map.GetNumberOfLabelObjects()

        # Map from label value to distance from principal axis. Used later to
        # fill in distance array.
        labels = dataset.active_scalars.flatten(order='F')
        max_label = np.max(labels)
        label_value_to_distance = [0 for i in range(max_label + 1)]
        for i in range(0, num_label_objects):
            label_object = shape_label_map.GetNthLabelObject(i)
            # Flip the centroid. I have verified that the x and z coordinates
            # of the centroid coming out of the shape label objects are swapped,
            # so I reverse it here.
            centroid = np.flipud(np.array(label_object.GetCentroid()))
            v = center - centroid
            dot = np.dot(v, axis)
            d = np.linalg.norm(v - dot*axis)
            label_value_to_distance[label_object.GetLabel()] = d

        distance = np.zeros(dataset.active_scalars.size)
        for i in range(len(labels)):
            distance[i] = label_value_to_distance[labels[i]]

        self.progress.value = STEP_PCT[3]

        dataset.active_scalars = copy.deepcopy(distance.reshape(data_shape,
                                                                order='F'))

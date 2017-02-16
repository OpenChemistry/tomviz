import tomviz.operators


class LabelObjectDistanceFromPrincipalAxis(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, label_value=1, principal_axis=0):
        """Computes the distance from the centroid of each connected component
        in the label object with the given label_value to the given principal
        axis and store that distance in each voxel of the label object connected
        component. A principal_axis of 0 is first principal axis, 1 is the
        second, and 2 is third.
        """

        import numpy as np
        from tomviz import itkutils
        from tomviz import utils

        self.progress.maximum = 100
        self.progress.value = 0

        STEP_PCT = [20, 60, 80, 100]

        fd = dataset.GetFieldData()
        axis_array = fd.GetArray('PrincipalAxes')
        assert axis_array is not None, \
            "Dataset does not have a PrincipalAxes field data array"
        assert axis_array.GetNumberOfTuples() == 3, \
            "PrincipalAxes array requires 3 tuples"
        assert axis_array.GetNumberOfComponents() == 3, \
            "PrincipalAxes array requires 3 components"
        assert principal_axis >= 0 and principal_axis <= 2, \
            "Invalid principal axis. Must be in range [0, 2]."

        axis = np.array(axis_array.GetTuple(principal_axis))

        center_array = fd.GetArray('Center')
        assert center_array is not None, \
            "Dataset does not have a Center field data array"
        assert center_array.GetNumberOfTuples() == 1, \
            "Center array requires 1 tuple"
        assert center_array.GetNumberOfComponents() == 3, \
            "Center array requires 3 components"

        center = np.array(center_array.GetTuple(0))

        # Blank out the undesired label values
        scalars = utils.get_scalars(dataset)
        scalars[scalars != label_value] = 0
        utils.set_scalars(dataset, scalars)
        self.progress.value = STEP_PCT[0]

        # Get connected components of voxels labeled by label value
        def connected_progress_func(fraction):
            self.progress.value = \
                int(fraction * (STEP_PCT[1] - STEP_PCT[0]) + STEP_PCT[0])
            return self.canceled

        utils.connected_components(dataset, 0, connected_progress_func)

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
        labels = utils.get_scalars(dataset)
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

        distance = np.zeros(dataset.GetNumberOfPoints())
        for i in range(len(labels)):
            distance[i] = label_value_to_distance[labels[i]]

        self.progress.value = STEP_PCT[3]

        import vtk.util.numpy_support as np_s
        distance_array = np_s.numpy_to_vtk(distance, deep=1)
        distance_array.SetName('Distance')
        dataset.GetPointData().SetScalars(distance_array)

import tomviz.operators


class BinaryThreshold(tomviz.operators.Operator):

    def transform(self, dataset, lower_threshold=40.0, upper_threshold=255.0):
        """Mark the voxels whose values lie between the lower and upper
        thresholds, inclusive, as 1 and everything else as 0, in a new
        child label map. The dataset passed in is left as it is."""
        import math

        import numpy as np

        array = dataset.active_scalars
        if array is None:
            raise RuntimeError('No data array found!')

        if np.issubdtype(array.dtype, np.integer):
            # Round inward so a fractional threshold keeps only the
            # integer values inside it, as the sliders show
            lower_threshold = math.ceil(lower_threshold)
            upper_threshold = math.floor(upper_threshold)
            if lower_threshold > upper_threshold:
                raise RuntimeError(
                    'No integer value lies between the lower and upper '
                    'thresholds')

        # NaN compares false on both sides, so it lands in the background
        inside = (array >= lower_threshold) & (array <= upper_threshold)
        label_map = dataset.create_child_dataset()
        label_map.active_scalars = np.asfortranarray(inside.astype(np.uint8))

        return {'thresholded_segmentation': label_map}

import tomviz.operators


class BinaryDilate(tomviz.operators.Operator):

    def transform(self, dataset, structuring_element_id=0, radius=1,
                  object_label=1, background_label=0):
        """Dilate segmented objects with a given label by a spherically
        symmetric structuring element with a given radius. Every voxel
        the dilation reaches takes the object label, whatever it held,
        as ITK's filter does."""
        import numpy as np
        from scipy import ndimage

        array = dataset.active_scalars
        if array is None:
            raise RuntimeError('No data array found!')

        structure = _structuring_element(structuring_element_id, radius)
        grown = ndimage.binary_dilation(array == object_label,
                                        structure=structure)
        result = array.copy()
        result[grown] = object_label
        dataset.active_scalars = np.asfortranarray(result)


def _structuring_element(shape_id, radius):
    """The neighbourhood ITK's FlatStructuringElement would build: a box,
    a ball (ellipsoid of the given radius) or a cross of axis lines."""
    import numpy as np

    radius = max(1, int(radius))
    size = 2 * radius + 1
    if shape_id == 0:
        return np.ones((size, size, size), dtype=bool)
    if shape_id == 1:
        grid = np.ogrid[-radius:radius + 1, -radius:radius + 1,
                        -radius:radius + 1]
        return sum((g / radius) ** 2 for g in grid) <= 1.0
    if shape_id == 2:
        cross = np.zeros((size, size, size), dtype=bool)
        cross[radius, radius, :] = True
        cross[radius, :, radius] = True
        cross[:, radius, radius] = True
        return cross
    raise RuntimeError('Invalid kernel shape id %d' % shape_id)

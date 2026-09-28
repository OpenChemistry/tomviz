import tomviz.operators


class UnsharpMask(tomviz.operators.Operator):

    def transform(self, dataset, amount=0.5, threshold=0.0, sigma=1.0):
        """Sharpen the image with the unsharp mask technique: the image is
        blurred with a Gaussian of the given sigma (in the data's physical
        units), and the difference between the image and its blur is
        scaled by the amount and added back. Where the difference is
        smaller than the threshold the voxel is left as it is, so noise is
        not amplified. The result is clamped to the range of the input's
        data type."""
        import numpy as np
        from scipy import ndimage

        array = dataset.active_scalars
        if array is None:
            raise RuntimeError('No data array found!')

        # Sigma is a physical length, as ITK's filter took it
        spacing = dataset.spacing if dataset.spacing is not None else (1,) * 3
        sigmas = [sigma / s if s > 0 else 0.0 for s in spacing]

        values = np.asarray(array, dtype=np.float64)
        blurred = ndimage.gaussian_filter(values, sigmas)
        detail = values - blurred
        sharpened = values + amount * detail
        if threshold > 0.0:
            keep = np.abs(detail) < threshold
            sharpened[keep] = values[keep]

        if np.issubdtype(array.dtype, np.integer):
            info = np.iinfo(array.dtype)
            sharpened = np.rint(np.clip(sharpened, info.min, info.max))
        result = sharpened.astype(array.dtype)
        dataset.active_scalars = np.asfortranarray(result)

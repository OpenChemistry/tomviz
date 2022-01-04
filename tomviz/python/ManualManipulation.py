def apply_shift(array, shift):
    if shift is None:
        return

    import numpy as np

    array[:] = np.roll(array, shift[0], axis=0)
    array[:] = np.roll(array, shift[1], axis=1)
    array[:] = np.roll(array, shift[2], axis=2)


def apply_rotation(array, rotation, spacing):
    import numpy as np

    if rotation is None or all(np.isclose(x, 0) for x in rotation):
        # No rotation. Nothing to do.
        return

    if not array.dtype == np.float32:
        raise Exception('Input array must be float32')

    # Wait to import ITK until we know we have to
    import itk

    # Need radians in the opposite direction...
    rotation = -np.radians(rotation)

    # Compute the center of rotation. Needs to match VTK.
    center = [x * y / 2 for x, y in zip(array.shape, spacing)]

    # Create the ITK image. We will convert to float.
    itk_image = itk.image_view_from_array(array)
    itk_image.SetSpacing(spacing)

    # Need to do the rotations in YXZ ordering.
    # The ITK Euler3DTransform class only allows ZXY and ZYX
    # ordering. So we will do the transform in two parts: one
    # with YX ordering and one with Z ordering.

    # FIXME: we should in theory be able to come up with a set of 3 angles
    # where we only have to apply a single Euler3DTransform, perhaps by
    # converting the Euler convention via scipy.spatial.transform.Rotation.
    # However, I have not yet figured out a way that works properly.
    axis_order = [(1, 0), (2,)]
    for axes in axis_order:
        angles = [0, 0, 0]
        for axis in axes:
            angles[axis] = rotation[axis]

        if all(np.isclose(x, 0) for x in angles):
            # No rotation. Skip it...
            continue

        t = itk.Euler3DTransform[itk.D].New()
        t.SetComputeZYX(True)
        t.SetRotation(*angles)
        t.SetCenter(center)

        image_type = itk.Image[itk.F, 3]
        filter = itk.ResampleImageFilter[image_type, image_type].New()
        filter.SetInput(itk_image)
        filter.SetTransform(t)
        filter.UseReferenceImageOn()
        filter.SetReferenceImage(itk_image)
        filter.Update()
        itk_image = filter.GetOutput()

    # Set the final result on the input numpy array
    array[:] = itk.array_view_from_image(itk_image).transpose([2, 1, 0])


def apply_resampling(array, spacing, reference_spacing):
    if not spacing or not reference_spacing:
        return array

    resampling_factor = [x / y for x, y in zip(spacing, reference_spacing)]

    import numpy as np

    if np.allclose(resampling_factor, 1):
        # Nothing to do
        return array

    from tomviz import utils

    from scipy.ndimage.interpolation import zoom

    # Transform the dataset.
    result_shape = utils.zoom_shape(array, resampling_factor)
    result = np.empty(result_shape, array.dtype, order='F')
    zoom(array, resampling_factor, output=result)

    return result


def apply_resize(array, reference_shape):
    # Resize the input array via cropping and padding to match the reference
    # shape.

    if array.shape == reference_shape:
        # Nothing to do...
        return array

    import numpy as np

    padding = []
    cropping = []
    for x, y in zip(array.shape, reference_shape):
        if y > x:
            total_pad = y - x
            half_pad = total_pad // 2
            remainder = total_pad % 2
            padding.append((half_pad, y - half_pad - remainder))
        else:
            padding.append((0, None))

        if y < x:
            total_crop = x - y
            half_crop = total_crop // 2
            remainder = total_crop % 2
            cropping.append((half_crop, x - half_crop - remainder))
        else:
            cropping.append((0, None))

    padding_slices = tuple([slice(*x) for x in padding])
    cropping_slices = tuple([slice(*x) for x in cropping])
    output = np.zeros(reference_shape, dtype=array.dtype)

    output[padding_slices] = array[cropping_slices]

    return output


def apply_alignment(array, spacing, reference_spacing, reference_shape):
    array = apply_resampling(array, spacing, reference_spacing)
    return apply_resize(array, reference_shape)


def transform(dataset, scaling=None, rotation=None, shift=None,
              align_with_reference=False, reference_spacing=None,
              reference_shape=None):
    array = dataset.active_scalars

    import numpy as np

    convert_to_float = (
        not all(np.isclose(x, 0) for x in rotation) and
        not array.dtype == np.float32
    )
    if convert_to_float:
        # If there are any rotations, we must convert to float32
        array = array.astype(np.float32)

    apply_rotation(array, rotation, scaling)
    apply_shift(array, shift)

    if align_with_reference:
        array = apply_alignment(array, scaling, reference_spacing,
                                reference_shape)
        scaling = reference_spacing

    dataset.active_scalars = array
    dataset.spacing = scaling

import numpy as np


def transform(
    dataset,
    transform_source='generate',
    padding=0,
    apply_to_all_arrays=True,
    # Only used if `transform_source` is `generate`
    transform_type='translation',
    reference='previous',
    transforms_save_file='',
    # Only used if `transform_type` is `slice_index`
    ref_slice_index=0,
    # Only used if `transform_source` is `from_file`.
    transform_file=None,
):
    kwargs = {
        'dataset': dataset,
        'padding': padding,
        'apply_to_all_arrays': apply_to_all_arrays,
        'axis': 2,  # axis hard-coded to two for now
    }
    if transform_source == 'generate':
        # Call the 'generate' version of this
        return transform_generate(
            transform_type=transform_type,
            reference=reference,
            ref_slice_index=ref_slice_index,
            transforms_save_file=transforms_save_file,
            **kwargs,
        )
    elif transform_source == 'from_file':
        # Call the 'from_file' version of this
        return transform_from_file(
            transform_file=transform_file,
            **kwargs,
        )

    raise Exception(f'Unknown transform source: {transform_source}')


def transform_generate(dataset, transform_type='translation', padding=0,
                       reference='previous', ref_slice_index=0,
                       apply_to_all_arrays=True, axis=0,
                       transforms_save_file=''):
    from pystackreg import StackReg

    array = dataset.active_scalars
    sr = StackReg(transform_type_map()[transform_type])

    # Apply padding if specified
    array = pad_array(array, padding, axis)

    # First, determine the transform matrices
    if reference != 'slice_index':
        tmats = sr.register_stack(
            array,
            axis=axis,
            reference=reference,
        )
    else:
        # We have to do our own special magic to handle this
        tmats = register_stack_from_slice(
            sr,
            array,
            ref_idx=ref_slice_index,
            axis=axis,
        )

    if transforms_save_file:
        np.savez_compressed(
            transforms_save_file,
            transform_type=transform_type,
            transform_matrices=tmats,
            spacing=[x for i, x in enumerate(dataset.spacing) if i != axis],
            shape=[x for i, x in enumerate(array.shape) if i != axis],
        )

    # Now apply the transform to all specified scalars
    apply_tmats(sr, dataset, tmats, padding, axis, apply_to_all_arrays)


def apply_tmats(sr, dataset, tmats, padding, axis, apply_to_all_arrays):
    if apply_to_all_arrays:
        names = dataset.scalars_names
    else:
        names = [dataset.active_name]

    for name in names:
        array = dataset.scalars(name)
        array = pad_array(array, padding, axis)
        output = sr.transform_stack(array, axis=axis, tmats=tmats)

        # Now remove the padding, if there was any
        output = depad_array(output, padding, axis)

        # Convert to Fortran to help VTK
        dataset.set_scalars(name, output)


def transform_from_file(dataset, transform_file, padding=0,
                        apply_to_all_arrays=True, axis=0):
    array = dataset.active_scalars

    # See if we need to rescale the transform matrices
    frame_shape = [x for i, x in enumerate(array.shape) if i != axis]
    spacing = [x for i, x in enumerate(dataset.spacing) if i != axis]

    with np.load(transform_file) as f:
        tmats = f['transform_matrices']
        transform_type = f['transform_type'].item()
        tmat_spacing = f['spacing']
        tmat_shape = f['shape']

    # Rescale the tmats so they can be applied to an image with different
    # physical extents (i. e., tmats generated from XRF can be applied to
    # ptychography, which has a higher resolution in voxel space).
    tmats = rescale_tmats(tmats, transform_type, tmat_shape, tmat_spacing,
                          frame_shape, spacing)

    from pystackreg import StackReg
    sr = StackReg(transform_type_map()[transform_type])
    apply_tmats(sr, dataset, tmats, padding, axis, apply_to_all_arrays)


def pad_array(array, padding, tilt_axis):
    if padding <= 0:
        return array

    pad_list = []
    for i in range(3):
        pad_list.append([0, 0] if i == tilt_axis else [padding, padding])

    return np.pad(array, pad_list)


def depad_array(array, padding, tilt_axis):
    if padding <= 0:
        return array

    slice_list = []
    for i in range(3):
        start = padding if i != tilt_axis else 0
        end = padding * -1 if i != tilt_axis else array.shape[i]
        slice_list.append(slice(start, end))
    return array[tuple(slice_list)]


def transform_type_map():
    from pystackreg import StackReg

    return {
        'translation': StackReg.TRANSLATION,
        'rigid body': StackReg.RIGID_BODY,
        'scaled rotation': StackReg.SCALED_ROTATION,
        'affine': StackReg.AFFINE,
        'bilinear': StackReg.BILINEAR,
    }


def register_stack_from_slice(sr, image_stack, ref_idx, axis):
    # PyStackReg doesn't allow us to pick an arbitrary slice to use
    # as the reference, but it does allow us to use the first slice.
    # To get around this, we'll swap whatever slice we want to use
    # as the reference with the first slice, then perform the registration,
    # then swap it back before we exit.
    # Swap the first row with the ref_idx. We'll unswap when we exit.
    if ref_idx == 0:
        # We don't have to do anything special actually...
        return sr.register_stack(image_stack, axis=axis, reference='first')

    slices = [slice(None)] * 3
    slices[axis] = [0, ref_idx]
    reversed_slices = slices.copy()
    reversed_slices[axis] = list(reversed(slices[axis]))
    image_stack[tuple(slices)] = image_stack[tuple(reversed_slices)]
    try:
        tmats = sr.register_stack(
            image_stack,
            axis=axis,
            reference='first',
        )
        # The first and reference idx are out of place. Swap them.
        tmats[[0, ref_idx]] = tmats[[ref_idx, 0]]
        return tmats
    finally:
        # Undo the row swap
        image_stack[tuple(slices)] = image_stack[tuple(reversed_slices)]


def rescale_tmats(tmats, transform_type, shape1, spacing1, shape2,
                  spacing2):
    if np.array_equal(shape1, shape2) and np.allclose(spacing1, spacing2):
        # No rescaling needed
        return tmats

    if transform_type != 'translation':
        msg = (
            'Shapes or spacings do not match. Currently, transformation '
            'matrices can only be rescaled if the transformation type is '
            '`translation`'
        )
        raise Exception(msg)

    # FIXME: do we need to use the shape at all?
    w1 = spacing1[0]
    h1 = spacing1[1]
    w2 = spacing2[0]
    h2 = spacing2[1]

    s = np.eye(3)
    s[0][0] = w2 / w1
    s[1][1] = h2 / h1

    return tmats @ s

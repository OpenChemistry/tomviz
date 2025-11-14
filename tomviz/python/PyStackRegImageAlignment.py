import numpy as np

from tomviz.utils import pad_array, depad_array


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
            padding=padding,
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
            padding=padding,
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


def transform_from_file(dataset, transform_file, apply_to_all_arrays=True,
                        axis=0):
    array = dataset.active_scalars

    # Padding should be zero if we transform from a file
    padding = 0

    # See if we need to rescale the transform matrices
    frame_shape = [x for i, x in enumerate(array.shape) if i != axis]
    spacing = [x for i, x in enumerate(dataset.spacing) if i != axis]

    with np.load(transform_file) as f:
        tmats = f['transform_matrices']
        transform_type = f['transform_type'].item()
        tmat_spacing = f['spacing']
        tmat_shape = f['shape']
        tmat_padding = f['padding']

    # Rescale the tmats so they can be applied to an image with different
    # physical extents (i. e., tmats generated from XRF can be applied to
    # ptychography, which has a higher resolution in voxel space).
    tmats = rescale_tmats(tmats, transform_type, tmat_shape, tmat_spacing,
                          tmat_padding, frame_shape, spacing)

    from pystackreg import StackReg
    sr = StackReg(transform_type_map()[transform_type])
    apply_tmats(sr, dataset, tmats, padding, axis, apply_to_all_arrays)


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


def rescale_tmats(tmats, transform_type, shape1, spacing1, padding1,
                  shape2, spacing2):
    if np.allclose(spacing1, spacing2) and padding1 == 0:
        # No rescaling needed
        return tmats

    if transform_type == 'bilinear':
        msg = (
            'Transformation matrix rescaling is not yet supported for '
            'bilinear transformations.'
        )
        raise Exception(msg)

    # Account for changes to padding
    pad_x_orig = padding1
    pad_y_orig = padding1
    pad_x_new = 0
    pad_y_new = 0

    # Account for pixel size changes
    pixel_ratio_x = spacing2[1] / spacing1[1]
    pixel_ratio_y = spacing2[0] / spacing1[0]

    # We need to account for the padding and pixel resizing changes
    translate_to_orig = np.array([
        [1, 0, pad_x_orig - pad_x_new],
        [0, 1, pad_y_orig - pad_y_new],
        [0, 0, 1],
    ])
    scale_to_orig = np.array([
        [pixel_ratio_x, 0, 0],
        [0, pixel_ratio_y, 0],
        [0, 0, 1],
    ])
    scale_to_new = np.array([
        [1 / pixel_ratio_x, 0, 0],
        [0, 1 / pixel_ratio_y, 0],
        [0, 0, 1],
    ])
    translate_to_new = np.array([
        [1, 0, -(pad_x_orig - pad_x_new)],
        [0, 1, -(pad_y_orig - pad_y_new)],
        [0, 0, 1],
    ])

    return (
        translate_to_new @
        scale_to_new @
        tmats @
        scale_to_orig @
        translate_to_orig
    )

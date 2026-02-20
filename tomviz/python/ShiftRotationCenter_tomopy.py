import numpy as np


def transform(dataset, rotation_center=0):
    from scipy.ndimage import shift as ndshift

    array = dataset.active_scalars
    tilt_axis = dataset.tilt_axis

    # rotation_center is an offset from the image midpoint.
    # A positive offset means the rotation center is right of center,
    # so we shift left (negative) to bring it to center.
    pixel_shift = -rotation_center

    # Shift the entire volume along the detector horizontal axis
    shift_vec = [0.0, 0.0, 0.0]
    shift_vec[1] = pixel_shift
    array = ndshift(array, shift_vec, mode='constant')

    dataset.active_scalars = array


def test_rotations(dataset, start=None, stop=None, steps=None, sli=0,
                   algorithm='gridrec', num_iter=15):
    # Get the current volume as a numpy array.
    array = dataset.active_scalars

    angles = dataset.tilt_angles
    tilt_axis = dataset.tilt_axis

    # TomoPy wants the tilt axis to be zero, so ensure that is true
    if tilt_axis == 2:
        array = np.transpose(array, [2, 0, 1])

    if angles is None:
        raise Exception('No angles found')

    recon_input = {
        'img_tomo': array,
        'angle': angles,
    }

    kwargs = {
        'f': recon_input,
        'start': start,
        'stop': stop,
        'steps': steps,
        'sli': sli,
        'algorithm': algorithm,
        'num_iter': num_iter,
    }

    # Perform the test rotations
    images, centers = rotcen_test(**kwargs)

    child = dataset.create_child_dataset()
    child.active_scalars = images

    return_values = {}
    return_values['images'] = child
    return_values['centers'] = centers.astype(float).tolist()
    return return_values


def rotcen_test(f, start=None, stop=None, steps=None, sli=0,
                algorithm='gridrec', num_iter=15):

    import tomopy

    tmp = np.array(f['img_tomo'][0])
    s = [1, tmp.shape[0], tmp.shape[1]]

    if sli == 0:
        sli = int(s[1] / 2)

    theta = np.array(f['angle']) / 180.0 * np.pi

    img_tomo = np.array(f['img_tomo'][:, sli:sli + 1, :])

    img_tomo[np.isnan(img_tomo)] = 0
    img_tomo[np.isinf(img_tomo)] = 0

    s = img_tomo.shape
    if len(s) == 2:
        img_tomo = img_tomo.reshape(s[0], 1, s[1])
        s = img_tomo.shape

    # Convert to absolute
    start_abs = int(round(s[2] / 2 + start))
    stop_abs = int(round(s[2] / 2 + stop))
    cen = np.linspace(start_abs, stop_abs, steps)
    img = np.zeros([len(cen), s[2], s[2]])

    recon_kwargs = {}
    if algorithm != 'gridrec':
        recon_kwargs['num_iter'] = num_iter

    for i in range(len(cen)):
        print(f'{i + 1}: rotcen {cen[i]}')
        img[i] = tomopy.recon(img_tomo, theta, center=cen[i],
                              algorithm=algorithm, **recon_kwargs)
    img = tomopy.circ_mask(img, axis=0, ratio=0.8)

    # Convert back to relative to the center
    cen -= s[2] / 2
    return img, cen

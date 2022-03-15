import numpy as np


def transform(dataset, rotation_center=0, slice_start=0, slice_stop=1,
              denoise_flag=0, denoise_level=9, dark_scale=1):

    # Get the current volume as a numpy array.
    array = dataset.active_scalars

    dark = dataset.dark
    white = dataset.white
    angles = dataset.tilt_angles
    tilt_axis = dataset.tilt_axis

    # TomoPy wants the tilt axis to be zero, so ensure that is true
    if tilt_axis == 2:
        order = [2, 1, 0]
        array = np.transpose(array, order)

        if dark is not None:
            dark = np.transpose(dark, order)

        if white is not None:
            white = np.transpose(white, order)

    if angles is None:
        raise Exception('No angles found')

    # FIXME: Are these right?
    recon_input = {
        'img_tomo': array,
        'angle': angles,
    }

    if dark is not None:
        recon_input['img_dark_avg'] = dark

    if white is not None:
        recon_input['img_bkg_avg'] = white

    kwargs = {
        'f': recon_input,
        'rot_cen': rotation_center,
        'sli': [slice_start, slice_stop],
        'denoise_flag': denoise_flag,
        'denoise_level': denoise_level,
        'dark_scale': dark_scale,
    }

    # Perform the reconstruction
    output = recon(**kwargs)

    # Set the transformed array
    child = dataset.create_child_dataset()
    child.active_scalars = output

    return_values = {}
    return_values['reconstruction'] = child
    return return_values


def test_rotations(dataset, start=None, stop=None, steps=None, sli=0,
                   denoise_flag=0, denoise_level=9, dark_scale=1):
    # Get the current volume as a numpy array.
    array = dataset.active_scalars

    dark = dataset.dark
    white = dataset.white
    angles = dataset.tilt_angles
    tilt_axis = dataset.tilt_axis

    # TomoPy wants the tilt axis to be zero, so ensure that is true
    if tilt_axis == 2:
        order = [2, 1, 0]
        array = np.transpose(array, order)

        if dark is not None:
            dark = np.transpose(dark, order)

        if white is not None:
            white = np.transpose(white, order)

    if angles is None:
        raise Exception('No angles found')

    recon_input = {
        'img_tomo': array,
        'angle': angles,
    }

    if dark is not None:
        recon_input['img_dark_avg'] = dark

    if white is not None:
        recon_input['img_bkg_avg'] = white

    kwargs = {
        'f': recon_input,
        'start': start,
        'stop': stop,
        'steps': steps,
        'sli': sli,
        'denoise_flag': denoise_flag,
        'denoise_level': denoise_level,
        'dark_scale': dark_scale,
    }

    if dark is None or white is None:
        kwargs['txm_normed_flag'] = True

    # Perform the reconstruction
    images, centers = rotcen_test(**kwargs)

    child = dataset.create_child_dataset()
    child.active_scalars = images

    return_values = {}
    return_values['images'] = child
    return_values['centers'] = centers.astype(float).tolist()
    return return_values


def find_nearest(data, value):
    data = np.array(data)
    return np.abs(data - value).argmin()


def recon(f, rot_cen, sli=[], binning=None, zero_flag=0, block_list=[],
          bkg_level=0, txm_normed_flag=0, read_full_memory=0, denoise_flag=0,
          denoise_level=9, dark_scale=1):
    '''
    reconstruct 3D tomography
    Inputs:
    --------
    f: dict
        input dictionary of scan
    rot_cen: float
        rotation center
    sli: list
        a range of slice to recontruct, e.g. [100:300]
    bingning: int
        binning the reconstruted 3D tomographic image
    zero_flag: bool
        if 1: set negative pixel value to 0
        if 0: keep negative pixel value
    block_list: list
        a list of index for the projections that will not be considered in
        reconstruction

    '''
    import tomopy

    tmp = np.array(f['img_tomo'][0])
    s = [1, tmp.shape[0], tmp.shape[1]]

    if len(sli) == 0:
        sli = [0, s[1]]
    elif len(sli) == 1 and sli[0] >= 0 and sli[0] <= s[1]:
        sli = [sli[0], sli[0]+1]
    elif len(sli) == 2 and sli[0] >= 0 and sli[1] <= s[1]:
        pass
    else:
        print('non valid slice id, will take reconstruction for the whole',
              'object')
    '''
    if len(col) == 0:
        col = [0, s[2]]
    elif len(col) == 1 and col[0] >=0 and col[0] <= s[2]:
        col = [col[0], col[0]+1]
    elif len(col) == 2 and col[0] >=0 and col[1] <= s[2]:
        col_info = '_col_{}_{}'.format(col[0], col[1])
    else:
        col = [0, s[2]]
        print('invalid col id, will take reconstruction for the whole object')
    '''
    # rot_cen = rot_cen - col[0]
    theta = np.array(f['angle']) / 180.0 * np.pi
    pos = find_nearest(theta, theta[0]+np.pi)
    block_list = list(block_list) + list(np.arange(pos+1, len(theta)))
    allow_list = list(set(np.arange(len(theta))) - set(block_list))
    theta = theta[allow_list]
    tmp = np.squeeze(np.array(f['img_tomo'][0]))
    s = tmp.shape

    sli_step = 40
    sli_total = np.arange(sli[0], sli[1])
    binning = binning if binning else 1

    n_steps = int(len(sli_total) / sli_step)
    rot_cen = rot_cen * 1.0 / binning

    if read_full_memory:
        sli_step = sli[1] - sli[0]
        n_steps = 1

    if denoise_flag:
        add_slice = min(sli_step // 2, 20)
        wiener_param = {}
        psf = 2
        wiener_param['psf'] = np.ones([psf, psf])/(psf**2)
        wiener_param['reg'] = None
        wiener_param['balance'] = 0.3
        wiener_param['is_real'] = True
        wiener_param['clip'] = True
    else:
        add_slice = 0
        wiener_param = []

    try:
        rec = np.zeros([s[0] // binning, s[1] // binning, s[1] // binning],
                       dtype=np.float32)
    except Exception:
        print('Cannot allocate memory')

    '''
    # first sli_step slices: will not do any denoising
    prj_norm = proj_normalize(f, [0, sli_step], txm_normed_flag, binning,
                              allow_list, bkg_level)
    prj_norm = wiener_denoise(prj_norm, wiener_param, denoise_flag)
    rec_sub = tomopy.recon(prj_norm, theta, center=rot_cen,
                           algorithm='gridrec')
    rec[0 : rec_sub.shape[0]] = rec_sub
    '''
    # following slices
    for i in range(n_steps):
        if i == n_steps-1:
            sli_sub = [i*sli_step+sli_total[0], len(sli_total)+sli[0]]
            current_sli = sli_sub
        else:
            sli_sub = [i*sli_step+sli_total[0], (i+1)*sli_step+sli_total[0]]
            current_sli = [sli_sub[0]-add_slice, sli_sub[1]+add_slice]
        print(f'recon {i+1}/{n_steps}:    sli = [{sli_sub[0]},',
              f'{sli_sub[1]}] ... ')
        prj_norm = proj_normalize(f, current_sli, txm_normed_flag, binning,
                                  allow_list, bkg_level, denoise_level,
                                  dark_scale)
        prj_norm = wiener_denoise(prj_norm, wiener_param, denoise_flag)
        if i != 0 and i != n_steps - 1:
            start = add_slice // binning
            stop = sli_step // binning + start
            prj_norm = prj_norm[:, start:stop]
        rec_sub = tomopy.recon(prj_norm, theta, center=rot_cen,
                               algorithm='gridrec')
        start = i * sli_step // binning
        rec[start: start + rec_sub.shape[0]] = rec_sub

    if zero_flag:
        rec[rec < 0] = 0

    return rec


def wiener_denoise(prj_norm, wiener_param, denoise_flag):
    import skimage.restoration as skr
    if not denoise_flag or not len(wiener_param):
        return prj_norm

    ss = prj_norm.shape
    psf = wiener_param['psf']
    reg = wiener_param['reg']
    balance = wiener_param['balance']
    is_real = wiener_param['is_real']
    clip = wiener_param['clip']
    for j in range(ss[0]):
        prj_norm[j] = skr.wiener(prj_norm[j], psf=psf, reg=reg,
                                 balance=balance, is_real=is_real, clip=clip)
    return prj_norm


def proj_normalize(f, sli, txm_normed_flag, binning, allow_list=[],
                   bkg_level=0, denoise_level=9, dark_scale=1):
    import tomopy

    img_tomo = np.array(f['img_tomo'][:, sli[0]:sli[1], :])
    try:
        img_bkg = np.array(f['img_bkg_avg'][:, sli[0]:sli[1]])
    except Exception:
        img_bkg = []
    try:
        img_dark = np.array(f['img_dark_avg'][:, sli[0]:sli[1]])
    except Exception:
        img_dark = []
    if len(img_dark) == 0 or len(img_bkg) == 0 or txm_normed_flag == 1:
        prj = img_tomo
    else:
        prj = ((img_tomo - img_dark / dark_scale) /
               (img_bkg - img_dark / dark_scale))

    s = prj.shape
    prj = bin_ndarray(prj, (s[0], int(s[1] / binning), int(s[2] / binning)),
                      'mean')
    prj_norm = -np.log(prj)
    prj_norm[np.isnan(prj_norm)] = 0
    prj_norm[np.isinf(prj_norm)] = 0
    prj_norm[prj_norm < 0] = 0
    prj_norm = prj_norm[allow_list]
    prj_norm = tomopy.prep.stripe.remove_stripe_fw(prj_norm,
                                                   level=denoise_level,
                                                   wname='db5', sigma=1,
                                                   pad=True)
    prj_norm -= bkg_level
    return prj_norm


def bin_ndarray(ndarray, new_shape=None, operation='mean'):
    """
    Bins an ndarray in all axes based on the target shape, by summing or
        averaging.

    Number of output dimensions must match number of input dimensions and
        new axes must divide old ones.

    Example
    -------
    >>> m = np.arange(0,100,1).reshape((10,10))
    >>> n = bin_ndarray(m, new_shape=(5,5), operation='sum')
    >>> print(n)

    [[ 22  30  38  46  54]
     [102 110 118 126 134]
     [182 190 198 206 214]
     [262 270 278 286 294]
     [342 350 358 366 374]]

    """
    if new_shape is None:
        s = np.array(ndarray.shape)
        s1 = np.int32(s / 2)
        new_shape = tuple(s1)
    operation = operation.lower()
    if operation not in ['sum', 'mean']:
        raise ValueError("Operation not supported.")
    if ndarray.ndim != len(new_shape):
        raise ValueError("Shape mismatch: {} -> {}".format(ndarray.shape,
                                                           new_shape))
    compression_pairs = [(d, c // d) for d, c in zip(new_shape,
                                                     ndarray.shape)]
    flattened = [x for p in compression_pairs for x in p]
    ndarray = ndarray.reshape(flattened)
    for i in range(len(new_shape)):
        op = getattr(ndarray, operation)
        ndarray = op(-1*(i+1))
    return ndarray


def rotcen_test(f, start=None, stop=None, steps=None, sli=0, block_list=[],
                print_flag=1, bkg_level=0, txm_normed_flag=0, denoise_flag=0,
                denoise_level=9, dark_scale=1):

    import tomopy

    tmp = np.array(f['img_tomo'][0])
    s = [1, tmp.shape[0], tmp.shape[1]]

    if denoise_flag:
        import skimage.restoration as skr
        addition_slice = 100
        psf = 2
        psf = np.ones([psf, psf])/(psf**2)
        reg = None
        balance = 0.3
        is_real = True
        clip = True
    else:
        addition_slice = 0

    if sli == 0:
        sli = int(s[1] / 2)

    sli_exp = [np.max([0, sli - addition_slice // 2]),
               np.min([sli + addition_slice // 2 + 1, s[1]])]

    theta = np.array(f['angle']) / 180.0 * np.pi

    img_tomo = np.array(f['img_tomo'][:, sli_exp[0]:sli_exp[1], :])

    if txm_normed_flag:
        prj = img_tomo
    else:
        img_bkg = np.array(f['img_bkg_avg'][:, sli_exp[0]:sli_exp[1], :])
        img_dark = np.array(f['img_dark_avg'][:, sli_exp[0]:sli_exp[1], :])
        prj = ((img_tomo - img_dark / dark_scale) /
               (img_bkg - img_dark / dark_scale))

    prj_norm = -np.log(prj)
    prj_norm[np.isnan(prj_norm)] = 0
    prj_norm[np.isinf(prj_norm)] = 0
    prj_norm[prj_norm < 0] = 0

    prj_norm -= bkg_level

    prj_norm = tomopy.prep.stripe.remove_stripe_fw(prj_norm,
                                                   level=denoise_level,
                                                   wname='db5', sigma=1,
                                                   pad=True)
    if denoise_flag:  # denoise using wiener filter
        ss = prj_norm.shape
        for i in range(ss[0]):
            prj_norm[i] = skr.wiener(prj_norm[i], psf=psf, reg=reg,
                                     balance=balance, is_real=is_real,
                                     clip=clip)

    s = prj_norm.shape
    if len(s) == 2:
        prj_norm = prj_norm.reshape(s[0], 1, s[1])
        s = prj_norm.shape

    pos = find_nearest(theta, theta[0]+np.pi)
    block_list = list(block_list) + list(np.arange(pos+1, len(theta)))
    if len(block_list):
        allow_list = list(set(np.arange(len(prj_norm))) - set(block_list))
        prj_norm = prj_norm[allow_list]
        theta = theta[allow_list]
    if start is None or stop is None or steps is None:
        start = int(s[2]/2-50)
        stop = int(s[2]/2+50)
        steps = 26
    cen = np.linspace(start, stop, steps)
    img = np.zeros([len(cen), s[2], s[2]])
    for i in range(len(cen)):
        if print_flag:
            print('{}: rotcen {}'.format(i+1, cen[i]))
        img[i] = tomopy.recon(prj_norm[:, addition_slice:addition_slice + 1],
                              theta, center=cen[i], algorithm='gridrec')
    img = tomopy.circ_mask(img, axis=0, ratio=0.8)
    return img, cen

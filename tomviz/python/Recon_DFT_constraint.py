import pyfftw
import numpy as np


def transform_scalars(dataset):
    """
    3D Reconstruct from a tilt series using constraint-based Direct Fourier
    Method
    """

    from tomviz import utils
    import numpy as np
    ###Niter###
    ###Niter_update_support###
    ###supportSigma###
    ###supportThreshold### #percent
    supportThreshold = supportThreshold / 100.0

    nonnegativeVoxels = True
    tilt_angles = utils.get_tilt_angles(dataset) #Get Tilt angles

    tilt_images = utils.get_array(dataset)
    if tilt_images is None:
        raise RuntimeError("No scalars found!")

    #Direct Fourier recon without constraints
    (recon, recon_F) \
        = dfm3(tilt_images, tilt_angles, np.size(tilt_images, 0) * 2)

    kr_cutoffs = np.linspace(0.05, 0.5, 10)
    #average Fourier magnitude of tilt series as a function of kr
    I_data = radial_average(tilt_images, kr_cutoffs)

    #Search for solutions that satisfy additional constraints
    recon = difference_map_update(recon_F, nonnegativeVoxels, I_data,
                                  kr_cutoffs, Niter, Niter_update_support,
                                  supportSigma, supportThreshold)

    print('Reconsruction Complete')

    # Set the result as the new scalars.
    utils.set_array(dataset, recon)

    # Mark dataset as volume
    utils.mark_as_volume(dataset)


def dfm3(input, angles, Npad):
    input = np.double(input)
    (Nx, Ny, Nproj) = input.shape
    angles = np.double(angles)
    pad_pre = int(np.ceil((Npad - Ny) / 2.0))
    pad_post = int(np.floor((Npad - Ny) / 2.0))

    # Initialization
    Nz = np.int(Ny / 2.0 + 1)
    w = np.zeros((Nx, Ny, Nz)) #store weighting factors
    v = pyfftw.n_byte_align_empty((Nx, Ny, Nz), 16, dtype='complex128')
    v = np.zeros(v.shape) + 1j * np.zeros(v.shape)
    recon = pyfftw.n_byte_align_empty((Nx, Ny, Ny), 16, dtype='float64')
    recon_fftw_object = pyfftw.FFTW(
        v, recon, direction='FFTW_BACKWARD', axes=(0, 1, 2))

    p = pyfftw.n_byte_align_empty((Nx, Npad), 16, dtype='float64')
    pF = pyfftw.n_byte_align_empty((Nx, Npad / 2 + 1), 16, dtype='complex128')
    p_fftw_object = pyfftw.FFTW(p, pF, axes=(0, 1))

    dk = np.double(Ny) / np.double(Npad)

    for a in range(0, Nproj):
        ang = angles[a] * np.pi / 180
        projection = input[:, :, a] #2D projection image
        p = np.lib.pad(projection, ((0, 0), (pad_pre, pad_post)),
                       'constant', constant_values=(0, 0)) #pad zeros
        p = np.fft.ifftshift(p)
        p_fftw_object.update_arrays(p, pF)
        p_fftw_object()

        probjection_f = pF.copy()
        if ang < 0:
            probjection_f = np.conj(pF.copy())
            probjection_f[1:, :] = np.flipud(probjection_f[1:, :])
            ang = np.pi + ang

        # Bilinear extrapolation
        for i in range(0, np.int(np.ceil(Npad / 2)) + 1):
            ky = i * dk
            #kz = 0
            ky_new = np.cos(ang) * ky #new coord. after rotation
            kz_new = np.sin(ang) * ky
            sy = abs(np.floor(ky_new) - ky_new) #calculate weights
            sz = abs(np.floor(kz_new) - kz_new)
            for b in range(1, 5): #bilinear extrapolation
                pz, py, weight = bilinear(kz_new, ky_new, sz, sy, Ny, b)
                if (py >= 0 and py < Ny and pz >= 0 and pz < Nz / 2 + 1):
                    w[:, py, pz] = w[:, py, pz] + weight
                    v[:, py, pz] = v[:, py, pz] + weight * probjection_f[:, i]

    v[w != 0] = v[w != 0] / w[w != 0]
    recon_F = v.copy()
    recon_fftw_object.update_arrays(v, recon)
    recon_fftw_object()
    recon = np.fft.fftshift(recon)
    return (recon, recon_F)

# Bilinear extrapolation


def bilinear(kz_new, ky_new, sz, sy, N, p):
    if p == 1:
        py = np.floor(ky_new)
        pz = np.floor(kz_new)
        weight = (1 - sy) * (1 - sz)
    elif p == 2:
        py = np.ceil(ky_new)
        pz = np.floor(kz_new)
        weight = sy * (1 - sz)
    elif p == 3:
        py = np.floor(ky_new)
        pz = np.ceil(kz_new)
        weight = (1 - sy) * sz
    elif p == 4:
        py = np.ceil(ky_new)
        pz = np.ceil(kz_new)
        weight = sy * sz
    if py < 0:
        py = N + py
    else:
        py = py
    return (pz, py, weight)


def radial_average(tiltseries, kr_cutoffs):
    (Nx, Ny, Nproj) = tiltseries.shape

    f = pyfftw.n_byte_align_empty((Nx, Ny / 2 + 1), 16, dtype='complex128')
    r = pyfftw.n_byte_align_empty((Nx, Ny), 16, dtype='float64')
    p_fftw_object = pyfftw.FFTW(r, f, axes=(0, 1))
    Ir = np.zeros(kr_cutoffs.size)
    I = np.zeros(kr_cutoffs.size)

    kx = np.fft.fftfreq(Nx)
    ky = np.fft.fftfreq(Ny)
    ky = ky[0:int(np.ceil(Ny / 2) + 1)]

    kX, kY = np.meshgrid(ky, kx)
    kR = np.sqrt(kY**2 + kX**2)

    for a in range(0, Nproj):
        r = tiltseries[:, :, a].copy().astype('float64')
        p_fftw_object.update_arrays(r, f)
        p_fftw_object.execute()
        shell = kR <= kr_cutoffs[0]
        I[0] = np.sum(np.absolute(f[shell]))
        I[0] = I[0] / np.sum(shell)
        for j in range(1, kr_cutoffs.size):
            shell = np.logical_and(kR > kr_cutoffs[j - 1], kR <= kr_cutoffs[j])
            I[j] = np.sum(np.absolute(f[shell]))
            I[j] = I[j] / np.sum(shell)
        Ir = Ir + I
    Ir = Ir / Nproj
    return Ir


def difference_map_update(constraint, nonnegativeVoxels, I_data, kr_cutoffs,
                          N_iter, N_update_support, supportSigma,
                          supportThreshold):
    (Nx, Ny, Nz) = constraint.shape
    #Note: Nz = np.int(Ny/2+1)
    Ntot = Nx * Ny * Ny
    f = pyfftw.n_byte_align_empty((Nx, Ny, Nz), 16, dtype='complex128')
    r = pyfftw.n_byte_align_empty((Nx, Ny, Ny), 16, dtype='float64')
    fft_forward = pyfftw.FFTW(r, f, axes=(0, 1, 2))
    fft_inverse = pyfftw.FFTW(f, r, direction='FFTW_BACKWARD', axes=(0, 1, 2))

    kx = np.fft.fftfreq(Nx)
    ky = np.fft.fftfreq(Ny)
    kz = ky[0:Nz]

    kX, kY, kZ = np.meshgrid(kx, ky, kz)
    kR = np.sqrt(kY**2 + kX**2 + kZ**2)

    sigma = 0.5 * supportSigma
    G = np.exp(-kR**2 / (2 * sigma**2))

    #create initial support using sw
    f = constraint * G
    fft_inverse.update_arrays(f, r)
    fft_inverse.execute()
    cutoff = np.amax(r) * supportThreshold
    support = r >= cutoff

    constraint[kR > kr_cutoffs[-1]] = 0

    x = np.random.rand(Nx, Ny, Ny) #initial solution
    for i in range(1, N_iter + 1):
        #print i
        #image space projection
        y1 = x.copy()

        if nonnegativeVoxels:
            y1[y1 < 0] = 0  #non-negative constraint

        y1[np.logical_not(support)] = 0 #support constraint

        #Fourier space projection
        y2 = 2 * y1 - x

        r = y2.copy()
        fft_forward.update_arrays(r, f)
        fft_forward.execute()

        f[kR > kr_cutoffs[-1]] = 0 #apply low pass filter
        f[constraint != 0] = constraint[constraint != 0] #data constraint

        #Fourier magnitude constraint
        #leave the inner shell unchanged
        for j in range(1, kr_cutoffs.size):
            shell = np.logical_and(kR > kr_cutoffs[j - 1], kR <= kr_cutoffs[j])
            shell[constraint != 0] = False
            I = np.sum(np.absolute(f[shell]))
            if I != 0:
                I = I / np.sum(shell)
                # lower magnitude for high frequency information to reduce
                # artifacts
                f[shell] = f[shell] / I * I_data[j] * 0.5

        fft_inverse.update_arrays(f, r)
        fft_inverse.execute()
        y2 = r.copy() / Ntot

        #update
        x = x + y2 - y1
        #print np.linalg.norm((y2-y1).flatten())/np.sqrt(Ntot)
        #update support
        if (i < N_iter and np.mod(i, N_update_support) == 0):
            print "updating support"
            recon = (y2 + y1) / 2
            r = recon.copy()
            fft_forward.update_arrays(r, f)
            fft_forward.execute()
            f = f * G
            fft_inverse.update_arrays(f, r)
            fft_inverse.execute()
            cutoff = np.amax(r) * supportThreshold
            support = r >= cutoff

    recon = (y2 + y1) / 2
    return np.fft.fftshift(recon)

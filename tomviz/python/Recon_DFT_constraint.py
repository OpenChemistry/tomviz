import pyfftw
import numpy as np
import tomviz.operators
import time


class ReconConstrintedDFMOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, Niter=None, Niter_update_support=None,
                  supportSigma=None, supportThreshold=None,
                  seed=0):
        """
        3D Reconstruct from a tilt series using constraint-based Direct Fourier
        Method
        """
        self.progress.maximum = 1

        rng = np.random.default_rng(seed=seed)

        supportThreshold = supportThreshold / 100.0

        nonnegativeVoxels = True
        tiltAngles = dataset.tilt_angles #Get Tilt angles

        if dataset.active_scalars is None:
            raise RuntimeError("No scalars found!")

        num_scalars = dataset.num_scalars
        scalars_names = dataset.scalars_names
        child_dataset = None

        self.progress.maximum = Niter * num_scalars

        for array_idx, array_name in enumerate(scalars_names):
            self.progress.message = f'{array_name}: Initialization'

            tiltSeries = dataset.scalars(array_name)

            #Direct Fourier recon without constraints
            (recon, recon_F) \
                = dfm3(tiltSeries, tiltAngles, np.size(tiltSeries, 1) * 2)

            kr_cutoffs = np.linspace(0.05, 0.5, 10)
            #average Fourier magnitude of tilt series as a function of kr
            I_data = radial_average(tiltSeries, kr_cutoffs)

            (Nx, Ny, Nz) = recon_F.shape
            #Note: Nz = np.int64(Ny/2+1)
            Ntot = Nx * Ny * Ny
            f = pyfftw.n_byte_align_empty((Nx, Ny, Nz), 16, dtype=np.complex64)
            r = pyfftw.n_byte_align_empty((Nx, Ny, Ny), 16, dtype=np.float32)
            fft_forward = pyfftw.FFTW(r, f, axes=(0, 1, 2))
            fft_inverse = pyfftw.FFTW(
                f, r, direction='FFTW_BACKWARD', axes=(0, 1, 2))

            kx = np.fft.fftfreq(Nx)
            ky = np.fft.fftfreq(Ny)
            kz = ky[0:Nz]

            kX, kY, kZ = np.meshgrid(ky, kx, kz)
            kR = np.sqrt(kY**2 + kX**2 + kZ**2)

            sigma = 0.5 * supportSigma
            G = np.exp(-kR**2 / (2 * sigma**2))

            #create initial support using sw
            f = (recon_F * G).astype(np.complex64)
            fft_inverse.update_arrays(f, r)
            fft_inverse.execute()
            cutoff = np.amax(r) * supportThreshold
            support = r >= cutoff

            recon_F[kR > kr_cutoffs[-1]] = 0

            x = rng.random((Nx, Ny, Ny)).astype(np.float32) #initial solution

            step = 0

            t0 = time.time()
            counter = 1
            etcMessage = 'Estimated time to complete: n/a'

            for i in range(Niter):
                if self.canceled:
                    return
                self.progress.message = f'{array_name}: Iteration No.%d/%d. ' % (
                    i + 1, Niter) + etcMessage

                #image space projection
                y1 = x.copy()

                if nonnegativeVoxels:
                    y1[y1 < 0] = 0  #non-negative constraint

                y1[np.logical_not(support)] = 0 #support constraint

                #Fourier space projection
                y2 = 2 * y1 - x
                r = y2.copy().astype(np.float32)
                fft_forward.update_arrays(r, f)
                fft_forward.execute()

                f[kR > kr_cutoffs[-1]] = 0 #apply low pass filter
                f[recon_F != 0] = recon_F[recon_F != 0] #data constraint

                #Fourier magnitude constraint
                #leave the inner shell unchanged
                for j in range(1, kr_cutoffs.size):
                    shell = np.logical_and(
                        kR > kr_cutoffs[j - 1], kR <= kr_cutoffs[j])
                    shell[recon_F != 0] = False
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

                #update support
                if (i < Niter and np.mod(i, Niter_update_support) == 0):
                    recon[:] = (y2 + y1) / 2
                    r = recon.copy()
                    fft_forward.update_arrays(r, f)
                    fft_forward.execute()
                    f = (f * G).astype(np.complex64)
                    fft_inverse.update_arrays(f, r)
                    fft_inverse.execute()
                    cutoff = np.amax(r) * supportThreshold
                    support = r >= cutoff
                step += 1
                self.progress.value = step + array_idx * Niter
                timeLeft = (time.time() - t0) / counter * (Niter - counter)
                counter += 1
                timeLeftMin, timeLeftSec = divmod(timeLeft, 60)
                timeLeftHour, timeLeftMin = divmod(timeLeftMin, 60)
                etcMessage = 'Estimated time to complete: %02d:%02d:%02d' % (
                    timeLeftHour, timeLeftMin, timeLeftSec)

            recon[:] = (y2 + y1) / 2
            recon[:] = np.fft.fftshift(recon)

            if child_dataset is None:
                child_dataset = dataset.create_child_dataset()

            child_dataset.set_scalars(array_name, recon)

        returnValues = {}
        returnValues["reconstruction"] = child_dataset
        return returnValues


def dfm3(input, angles, Npad):
    input = np.double(input)
    (Nx, Ny, Nproj) = input.shape
    angles = np.double(angles)
    pad_pre = int(np.ceil((Npad - Ny) / 2.0))
    pad_post = int(np.floor((Npad - Ny) / 2.0))

    # Initialization
    Nz = Ny // 2 + 1
    w = np.zeros((Nx, Ny, Nz), dtype=np.float32) #store weighting factors
    v = pyfftw.n_byte_align_empty((Nx, Ny, Nz), 16, dtype=np.complex64)
    v = np.zeros(v.shape, dtype=np.float32) \
        + 1j * np.zeros(v.shape, dtype=np.float32)
    recon = pyfftw.n_byte_align_empty(
        (Nx, Ny, Ny), 16, dtype=np.float32, order='F')
    recon_fftw_object = pyfftw.FFTW(
        v, recon, direction='FFTW_BACKWARD', axes=(0, 1, 2))

    p = pyfftw.n_byte_align_empty((Nx, Npad), 16, dtype=np.float32)
    pF = pyfftw.n_byte_align_empty((Nx, Npad // 2 + 1), 16, dtype=np.complex64)
    p_fftw_object = pyfftw.FFTW(p, pF, axes=(0, 1))

    dk = np.double(Ny) / np.double(Npad)

    for a in range(0, Nproj):
        ang = angles[a] * np.pi / 180
        projection = input[:, :, a].astype(np.float32) #2D projection image
        p = np.pad(projection, ((0, 0), (pad_pre, pad_post)),
                   'constant', constant_values=(0, 0)) #pad zeros
        p = np.ascontiguousarray(np.fft.ifftshift(p))
        p_fftw_object.update_arrays(p, pF)
        p_fftw_object()

        probjection_f = pF.copy()
        if ang < 0:
            probjection_f = np.conj(pF.copy())
            probjection_f[1:, :] = np.flipud(probjection_f[1:, :])
            ang = np.pi + ang

        # Bilinear extrapolation
        for i in range(0, np.int64(np.ceil(Npad / 2)) + 1):
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
    recon[:] = np.fft.fftshift(recon)
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
    return (int(pz), int(py), weight)


def radial_average(tiltseries, kr_cutoffs):
    (Nx, Ny, Nproj) = tiltseries.shape

    f = pyfftw.n_byte_align_empty((Nx, Ny // 2 + 1), 16, dtype=np.complex64)
    r = pyfftw.n_byte_align_empty((Nx, Ny), 16, dtype=np.float32)
    p_fftw_object = pyfftw.FFTW(r, f, axes=(0, 1))
    Ir = np.zeros(kr_cutoffs.size)
    I = np.zeros(kr_cutoffs.size)

    kx = np.fft.fftfreq(Nx)
    ky = np.fft.fftfreq(Ny)
    ky = ky[0:Ny // 2 + 1]

    kX, kY = np.meshgrid(ky, kx)
    kR = np.sqrt(kY**2 + kX**2)

    for a in range(0, Nproj):
        r = tiltseries[:, :, a].copy().astype(np.float32)
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

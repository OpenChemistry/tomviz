import pyfftw
import numpy as np
import tomviz.operators
import time


class ReconDFMOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        """3D Reconstruct from a tilt series using Direct Fourier Method"""

        self.progress.maximum = 1

        # Get Tilt angles
        tiltAngles = dataset.tilt_angles

        tiltSeries = dataset.active_scalars
        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        tiltSeries = np.double(tiltSeries)
        (Nx, Ny, Nproj) = tiltSeries.shape
        Npad = Ny * 2

        tiltAngles = np.double(tiltAngles)
        pad_pre = int(np.ceil((Npad - Ny) / 2.0))
        pad_post = int(np.floor((Npad - Ny) / 2.0))

        # Initialization
        self.progress.message = 'Initialization'
        Nz = Ny
        w = np.zeros((Nx, Ny, Nz // 2 + 1)) #store weighting factors
        v = pyfftw.empty_aligned(
            (Nx, Ny, Nz // 2 + 1), dtype='complex64', n=16)

        p = pyfftw.empty_aligned((Nx, Npad), dtype='float32', n=16)
        pF = pyfftw.empty_aligned(
            (Nx, Npad // 2 + 1), dtype='complex64', n=16)
        p_fftw_object = pyfftw.FFTW(p, pF, axes=(0, 1))

        dk = np.double(Ny) / np.double(Npad)

        self.progress.maximum = Nproj + 1
        step = 0

        t0 = time.time()
        etcMessage = 'Estimated time to complete: n/a'
        counter = 1
        for a in range(Nproj):
            if self.canceled:
                return
            self.progress.message = 'Tilt image No.%d/%d. ' % (
                a + 1, Nproj) + etcMessage

            ang = tiltAngles[a] * np.pi / 180
            projection = tiltSeries[:, :, a] #2D projection image
            p = np.lib.pad(projection, ((0, 0), (pad_pre, pad_post)),
                           'constant', constant_values=(0, 0)) #pad zeros
            p = np.float32(np.fft.ifftshift(p))
            p_fftw_object.update_arrays(p, pF)
            p_fftw_object()
            p = None #Garbage collector (gc)

            if ang < 0:
                pF = np.conj(pF)
                pF[1:, :] = np.flipud(pF[1:, :])
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
                        v[:, py, pz] = v[:, py, pz] + \
                            weight * pF[:, i]
            step += 1
            self.progress.value = step
            timeLeft = (time.time() - t0) / counter * (Nproj - counter)
            counter += 1
            timeLeftMin, timeLeftSec = divmod(timeLeft, 60)
            timeLeftHour, timeLeftMin = divmod(timeLeftMin, 60)
            etcMessage = 'Estimated time to complete: %02d:%02d:%02d' % (
                timeLeftHour, timeLeftMin, timeLeftSec)

        p = pF = None #gc

        self.progress.message = 'Inverse Fourier transform'
        v_temp = v.copy()
        recon = pyfftw.empty_aligned(
            (Nx, Ny, Nz), dtype='float32', order='F', n=16)
        recon_fftw_object = pyfftw.FFTW(
            v_temp, recon, direction='FFTW_BACKWARD', axes=(0, 1, 2))
        v[w != 0] = v[w != 0] / w[w != 0]
        recon_fftw_object.update_arrays(v, recon)
        v = v_temp = []    #gc
        recon_fftw_object()
        recon[:] = np.fft.fftshift(recon)

        step += 1
        self.progress.value = step

        self.progress.message = 'Passing data to Tomviz'

        child = dataset.create_child_dataset()
        child.active_scalars = recon

        returnValues = {}
        returnValues["reconstruction"] = child
        return returnValues


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

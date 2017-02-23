import pyfftw
import numpy as np
import tomviz.operators


class ReconDFMOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """3D Reconstruct from a tilt series using Direct Fourier Method"""

        self.progress.maximum = 1

        from tomviz import utils
        import numpy as np

        # Get Tilt angles
        tiltAngles = utils.get_tilt_angles(dataset)

        tiltSeries = utils.get_array(dataset)
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
        v = pyfftw.n_byte_align_empty(
            (Nx, Ny, Nz // 2 + 1), 16, dtype='complex128')
        v = np.zeros(v.shape) + 1j * np.zeros(v.shape)
        recon = pyfftw.n_byte_align_empty((Nx, Ny, Nz), 16, dtype='float64')
        recon_fftw_object = pyfftw.FFTW(
            v, recon, direction='FFTW_BACKWARD', axes=(0, 1, 2))

        p = pyfftw.n_byte_align_empty((Nx, Npad), 16, dtype='float64')
        pF = pyfftw.n_byte_align_empty(
            (Nx, Npad // 2 + 1), 16, dtype='complex128')
        p_fftw_object = pyfftw.FFTW(p, pF, axes=(0, 1))

        dk = np.double(Ny) / np.double(Npad)

        self.progress.maximum = Nproj + 1
        step = 0

        for a in range(Nproj):
            if self.canceled:
                return
            self.progress.message = 'Tilt image No.%d/%d' % (a + 1, Nproj)

            #print angles[a]
            ang = tiltAngles[a] * np.pi / 180
            projection = tiltSeries[:, :, a] #2D projection image
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
                        v[:, py, pz] = v[:, py, pz] + \
                            weight * probjection_f[:, i]
            step += 1
            self.progress.value = step

        self.progress.message = 'Inverse Fourier transform'
        v[w != 0] = v[w != 0] / w[w != 0]
        recon_fftw_object.update_arrays(v, recon)
        recon_fftw_object()
        recon = np.fft.fftshift(recon)

        step += 1
        self.progress.value = step

        from vtk import vtkImageData
        recon_dataset = vtkImageData()
        recon_dataset.CopyStructure(dataset)
        utils.set_array(recon_dataset, recon)
        utils.mark_as_volume(recon_dataset)

        returnValues = {}
        returnValues["reconstruction"] = recon_dataset
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

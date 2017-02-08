import numpy as np
from scipy.interpolate import interp1d
import tomviz.operators


class AutoTiltAxisShiftAlignmentOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """Automatic align the tilt axis to the center of images"""
        self.progress.maximum = 1

        from tomviz import utils
        # Get Tilt angles
        tilt_angles = utils.get_tilt_angles(dataset)

        tiltSeries = utils.get_array(dataset)
        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        Nx, Ny, Nz = tiltSeries.shape

        shifts = (np.linspace(-20, 20, 41)).astype('int')
        numberOfSlices = 5  # number of slices used for recon

        # randomly choose slices with top 50% total intensities
        tiltSeriesSum = np.sum(tiltSeries, axis=(1, 2))
        temp = tiltSeriesSum.argsort()[Nx // 2:]
        slices = temp[np.random.permutation(temp.size)[:numberOfSlices]]
        print('Reconstruction slices:')
        print(slices)

        I = np.zeros(shifts.size)

        self.progress.maximum = shifts.size - 1
        step = 0

        for i in range(shifts.size):
            if self.canceled:
                return
            shiftedTiltSeries = np.roll(
                tiltSeries[slices, :, :, ], shifts[i], axis=1)
            for s in range(numberOfSlices):
                self.progress.message =
                    'Reconstructing slice No.%d with %d pixels shift' %
                    (slices[s], shifts[i])
                recon = wbp2(shiftedTiltSeries[s, :, :],
                             tilt_angles, Ny, 'ramp', 'linear')
                I[i] = I[i] + np.amax(recon)

            step += 1
            self.progress.value = step

        print('shift: %d' % shifts[np.argmax(I)])

        result = np.roll(tiltSeries, shifts[np.argmax(I)], axis=1)

        # Set the result as the new scalars.
        utils.set_array(dataset, result)


def wbp2(sinogram, angles, N=None, filter="ramp", interp="linear"):
    if sinogram.ndim != 2:
        raise ValueError('Sinogram must be 2D')
    (Nray, Nproj) = sinogram.shape
    if Nproj != angles.size:
        raise ValueError('Sinogram does not match angles!')

    interpolation_methods = ('linear', 'nearest', 'spline', 'cubic')
    if interp not in interpolation_methods:
        raise ValueError("Unknown interpolation: %s" % interp)
    if not N:  # if ouput size is not given
        N = int(np.floor(np.sqrt(Nray**2 / 2.0)))

    ang = np.double(angles) * np.pi / 180.0
    # Create Fourier filter
    F = makeFilter(Nray, filter)
    # Pad sinogram for filtering
    s = np.lib.pad(sinogram, ((0, F.size - Nray), (0, 0)),
                   'constant', constant_values=(0, 0))
    # Apply Fourier filter
    s = np.fft.fft(s, axis=0) * F
    s = np.real(np.fft.ifft(s, axis=0))
    # Change back to original
    s = s[:Nray, :]

    # Back projection
    recon = np.zeros((N, N))
    center_proj = Nray // 2  # Index of center of projection
    [X, Y] = np.mgrid[0:N, 0:N]
    xpr = X - int(N) // 2
    ypr = Y - int(N) // 2

    for j in range(Nproj):
        t = ypr * np.cos(ang[j]) - xpr * np.sin(ang[j])
        x = np.arange(Nray) - center_proj
        if interp == 'linear':
            bp = np.interp(t, x, s[:, j], left=0, right=0)
        elif interp == 'spline':
            interpolant = interp1d(
                x, s[:, j], kind='slinear', bounds_error=False, fill_value=0)
            bp = interpolant(t)
        else:
            interpolant = interp1d(
                x, s[:, j], kind=interp, bounds_error=False, fill_value=0)
            bp = interpolant(t)
        recon = recon + bp

    # Normalize
    recon = recon * np.pi / 2 / Nproj
    return recon

# Filter (1D) projections.


def makeFilter(Nray, filterMethod="ramp"):
    # Calculate next power of 2
    N2 = 2**np.ceil(np.log2(Nray))
    # Make a ramp filter.
    freq = np.fft.fftfreq(int(N2)).reshape(-1, 1)
    omega = 2 * np.pi * freq
    filter = 2 * np.abs(freq)

    if filterMethod == "ramp":
        pass
    elif filterMethod == "shepp-logan":
        filter[1:] = filter[1:] * np.sin(omega[1:]) / omega[1:]
    elif filterMethod == "cosine":
        filter[1:] = filter[1:] * np.cos(filter[1:])
    elif filterMethod == "hamming":
        filter[1:] = filter[1:] * (0.54 + 0.46 * np.cos(omega[1:] / 2))
    elif filterMethod == "hann":
        filter[1:] = filter[1:] * (1 + np.cos(omega[1:] / 2)) / 2
    elif filterMethod == "none":
        filter[:] = 1
    else:
        raise ValueError("Unknown filter: %s" % filterMethod)

    return filter

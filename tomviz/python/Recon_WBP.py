import numpy as np
from scipy.interpolate import interp1d
import tomviz.operators
import time


class ReconWBPOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, Nrecon=None, filter=None, interp=None,
                  Nupdates=None):
        """
        3D Reconstruct from a tilt series using Weighted Back-projection Method
        """
        self.progress.maximum = 1

        interpolation_methods = ('linear', 'nearest', 'spline', 'cubic')
        filter_methods = ('none', 'ramp', 'shepp-logan',
                          'cosine', 'hamming', 'hann')

        # Get Tilt angles
        tilt_angles = dataset.tilt_angles
        tiltSeries = dataset.active_scalars
        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        Nslice = tiltSeries.shape[0]

        self.progress.maximum = Nslice
        step = 0

        recon = np.empty([Nslice, Nrecon, Nrecon], dtype=np.float32, order='F')
        t0 = time.time()
        counter = 1
        etcMessage = 'Estimated time to complete: n/a'

        child = dataset.create_child_dataset() #create child for recon

        for i in range(Nslice):
            if self.canceled:
                return
            self.progress.message = 'Slice No.%d/%d. ' % (
                i + 1, Nslice) + etcMessage

            recon[i, :, :] = wbp2(tiltSeries[i, :, :], tilt_angles, Nrecon,
                                  filter_methods[filter],
                                  interpolation_methods[interp])
            step += 1
            self.progress.value = step
            timeLeft = (time.time() - t0) / counter * (Nslice - counter)
            counter += 1
            timeLeftMin, timeLeftSec = divmod(timeLeft, 60)
            timeLeftHour, timeLeftMin = divmod(timeLeftMin, 60)
            etcMessage = 'Estimated time to complete: %02d:%02d:%02d' % (
                timeLeftHour, timeLeftMin, timeLeftSec)

            # Update only once every so many steps
            if Nupdates != 0 and (i + 1) % (Nslice//Nupdates) == 0:
                child.active_scalars = recon #add recon to child
                # This copies data to the main thread
                self.progress.data = child

        # One last update of the child data.
        child.active_scalars = recon #add recon to child
        self.progress.data = child

        returnValues = {}
        returnValues["reconstruction"] = child
        return returnValues


def wbp2(sinogram, angles, N=None, filter="ramp", interp="linear"):
    if sinogram.ndim != 2:
        raise ValueError('Sinogram must be 2D')
    (Nray, Nproj) = sinogram.shape
    if Nproj != angles.size:
        raise ValueError('Sinogram does not match angles!')

    interpolation_methods = ('linear', 'nearest', 'spline', 'cubic')
    if interp not in interpolation_methods:
        raise ValueError("Unknown interpolation: %s" % interp)
    if not N:  # if output size is not given
        N = int(np.floor(np.sqrt(Nray**2 / 2.0)))

    ang = np.double(angles) * np.pi / 180.0
    # Create Fourier filter
    F = makeFilter(Nray, filter)
    # Pad sinogram for filtering
    s = np.pad(sinogram, ((0, F.size - Nray), (0, 0)),
               'constant', constant_values=(0, 0))
    # Apply Fourier filter
    s = np.fft.fft(s, axis=0) * F
    s = np.real(np.fft.ifft(s, axis=0))
    # Change back to original
    s = s[:Nray, :]

    # Back projection
    recon = np.zeros((N, N), np.float32)
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

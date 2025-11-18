from scipy.interpolate import interp1d
import numpy as np


class WBP:

    def __init__(self, Nx, Ny, Nz):

        self.Nslice = Nx
        self.Nray = Ny
        self.Nproj = Nz

        self.recon = np.empty([self.Nslice, self.Nray, self.Nray],
                              dtype=np.float32, order='F')

    def set_tilt_series(self, input_projections, input_tilt_angles):

        self.tiltSeries = input_projections
        self.tiltAngles = input_tilt_angles

        self.Nproj = self.tiltSeries.shape[2]

    def WBP(self, sliceInd):

        # Main Reconstruction Loop
        self.recon[sliceInd, :, :] = self.wbp2(self.tiltSeries[sliceInd, :, :])

    def wbp2(self, sinogram, filter="ramp", interp="linear"):

        ang = np.double(self.tiltAngles) * np.pi / 180.0
        # Create Fourier filter
        F = self.makeFilter(filter)
        # Pad sinogram for filtering
        s = np.pad(sinogram, ((0, F.size - self.Nray), (0, 0)),
                   'constant', constant_values=(0, 0))
        # Apply Fourier filter
        s = np.fft.fft(s, axis=0) * F
        s = np.real(np.fft.ifft(s, axis=0))
        # Change back to original
        s = s[:self.Nray, :]

        # Back projection
        reconSlice = np.zeros((self.Nray, self.Nray), np.float32)
        center_proj = self.Nray // 2  # Index of center of projection
        [X, Y] = np.mgrid[0:self.Nray, 0:self.Nray]
        xpr = X - int(self.Nray) // 2
        ypr = Y - int(self.Nray) // 2

        for j in range(self.Nproj):
            t = ypr * np.cos(ang[j]) - xpr * np.sin(ang[j])
            x = np.arange(self.Nray) - center_proj
            if interp == 'linear':
                bp = np.interp(t, x, s[:, j], left=0, right=0)
            elif interp == 'spline':
                interpolant = interp1d(
                    x, s[:, j], kind='slinear', bounds_error=False,
                    fill_value=0)
                bp = interpolant(t)
            else:
                interpolant = interp1d(
                    x, s[:, j], kind=interp, bounds_error=False, fill_value=0)
                bp = interpolant(t)
            reconSlice = reconSlice + bp

        # Normalize
        reconSlice = reconSlice * np.pi / 2 / self.Nproj
        reconSlice[reconSlice < 0] = 0
        return reconSlice

    def makeFilter(self, filterMethod="ramp"):
        # Calculate next power of 2
        N2 = 2**np.ceil(np.log2(self.Nray))
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

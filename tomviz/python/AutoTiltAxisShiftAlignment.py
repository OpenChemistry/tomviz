import numpy as np
from scipy.interpolate import interp1d
import tomviz.operators

def transform_scalars(dataset):
    """
      xxx
    """

    from tomviz import utils
    # Get Tilt angles
    tilt_angles = utils.get_tilt_angles(dataset)

    tiltSeries = utils.get_array(dataset)
    if tiltSeries is None:
        raise RuntimeError("No scalars found!")

    Nrecon = tiltSeries.shape[1]
    tiltSeriesSum = np.sum(tiltSeries,axis=(1,2))
    #reconSlices = tiltSeriesSum.argsort()[-5:][::-1]
    #reconSlices = np.array([-30,-20,-10, 0, 10, 20, 30])
    #reconSlices = reconSlices + tiltSeries.shape[0] // 2
    #reconSlices = np.random.permutation(tiltSeries.shape[0])[:5]
    temp = tiltSeriesSum.argsort()[tiltSeries.shape[0] // 2:]
    reconSlices = temp[np.random.permutation(temp.size)[:5]]
    print reconSlices
  
    shifts = (np.linspace(-20,20,41)).astype('int')
    recon = np.zeros((Nrecon, Nrecon))
    I = np.zeros(shifts.size)
    
    for s in reconSlices:
        print s
        slice = tiltSeries[s, :, :]
        #tiltSeries_shifted  = np.roll(tiltSeries, shifts[i], axis=1)
        for i in range(shifts.size):
            sliceShifted = np.roll(slice, shifts[i], axis=0)
            recon = wbp2(sliceShifted,tilt_angles, Nrecon,'ramp','linear')
            I[i] = np.amax(recon)
        print shifts[np.argmax(I)]

    print "Done"

    for i in range(shifts.size):
        tiltSeries_shifted  = np.roll(tiltSeries, shifts[i], axis=1)
        for s in reconSlices:
            recon = wbp2(tiltSeries_shifted[s, :, :],
                   tilt_angles, Nrecon, 'ramp','linear')
            I[i] = I[i] + np.amax(recon)

        print shifts[i], I[i]
    print shifts[np.argmax(I)]

    tiltSeries_shifted  = np.roll(tiltSeries, shifts[np.argmax(I)], axis=1)
    #tiltSeries_shifted = tiltSeries
    # Set the result as the new scalars.
    utils.set_array(dataset, tiltSeries_shifted)

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

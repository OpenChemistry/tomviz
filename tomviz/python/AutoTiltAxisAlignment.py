from tomviz import utils
import numpy as np
from scipy import ndimage


def transform_scalars(dataset):
    """Automatic align the tilt axis of tilt series to the center of images"""

    # Get Tilt Series
    tiltSeries = utils.get_array(dataset)

    if tiltSeries is None: #Check if data exists
        raise RuntimeError("No data array found!")

    (Nslice, Nray, Nproj) = tiltSeries.shape
    Intensity = np.zeros(tiltSeries.shape)

    #take FFT of all projections
    for i in range(Nproj):
        tiltImage = tiltSeries[:, :, i]
        tiltImage_F = np.abs(np.fft.fft2(tiltImage))
        if (i == 0):
            a = tiltImage_F[0, 0]
        Intensity[:, :, i] = np.fft.fftshift(tiltImage_F / a)

    #rescale intensity
    Intensity = np.power(Intensity, 0.2)

    #calculate variation itensity image
    Intensity_var = np.var(Intensity, axis=2)
    #start with coarse search
    coarse_step = 2
    angles = np.arange(-90, 90, coarse_step)
    rot_ang = find_min_line(Intensity_var, angles)

    #now refine
    fine_step = 0.1
    angles = np.arange(rot_ang - coarse_step, rot_ang + coarse_step, fine_step)
    rot_ang = find_min_line(Intensity_var, angles)
    print "rotate tilt series by", -rot_ang, "degrees"
    tiltSeries_rot = ndimage.interpolation.rotate(tiltSeries, -rot_ang, axes=((0, 1)))

    # Set the result as the new scalars.
    utils.set_array(dataset, tiltSeries_rot)


def find_min_line(Intensity_var, angles):
    Nx = Intensity_var.shape[0]; Ny = Intensity_var.shape[1]
    cenx = np.floor(Nx / 2); ceny = np.floor(Ny / 2)

    N = np.round(np.min([Nx, Ny]) / 3)
    Intensity_line = np.zeros((angles.size, N))
    for a in range(0, angles.size):
        ang = angles[a] * np.pi / 180
        w = np.zeros(N)
        v = np.zeros(N)
        for i in range(0, N):
            x = i * np.cos(ang); y = i * np.sin(ang);
            sx = abs(np.floor(x) - x); sy = abs(np.floor(y) - y)
            px = np.floor(x) + cenx; py = np.floor(y) + ceny
            if (px >= 0 and px < Nx and py >= 0 and py < Ny):
                w[i] = w[i] + (1 - sx) * (1 - sy)
                v[i] = v[i] + (1 - sx) * (1 - sy) * Intensity_var[px, py]
            px = np.ceil(x) + cenx; py = np.floor(y) + ceny
            if (px >= 0 and px < Nx and py >= 0 and py < Ny):
                w[i] = w[i] + sx * (1 - sy)
                v[i] = v[i] + sx * (1 - sy) * Intensity_var[px, py]
            px = np.floor(x) + cenx; py = np.ceil(y) + ceny
            if (px >= 0 and px < Nx and py >= 0 and py < Ny):
                w[i] = w[i] + (1 - sx) * sy
                v[i] = v[i] + (1 - sx) * sy * Intensity_var[px, py]
            px = np.ceil(x) + cenx; py = np.ceil(y) + ceny
            if (px >= 0 and px < Nx and py >= 0 and py < Ny):
                w[i] = w[i] + sx * sy
                v[i] = v[i] + sx * sy * Intensity_var[px, py]
        v[w != 0] = v[w != 0] / w[w != 0]
        Intensity_line[a, :] = v
    Intensity_line_sum = np.sum(Intensity_line, axis=1)
    rot_ang = angles[np.argmin(Intensity_line_sum)]

    return rot_ang

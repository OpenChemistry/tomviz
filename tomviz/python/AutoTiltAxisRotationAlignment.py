from tomviz import utils
import numpy as np
from scipy import ndimage
import tomviz.operators


class AutoTiltAxisRotationAlignOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """
          Automatic align the tilt axis to horizontal direction (x-axis)
        """
        self.progress.maximum = 1

        # Get Tilt Series
        tiltSeries = utils.get_array(dataset)

        if tiltSeries is None: #Check if data exists
            raise RuntimeError("No data array found!")

        (Nslice, Nray, Nproj) = tiltSeries.shape
        Intensity = np.zeros(tiltSeries.shape)

        self.progress.maximum = Nproj + 131 #coarse(90)+fine(41)
        step = 0

        self.progress.message = 'Initialization'
        #take FFT of all projections
        for i in range(Nproj):
            self.progress.message = ('Taking Fourier transofrm of tilt image'
                                     'No.%d/%d' % (i + 1, Nproj))
            tiltImage = tiltSeries[:, :, i]
            tiltImage_F = np.abs(np.fft.fft2(tiltImage))
            if (i == 0):
                temp = tiltImage_F[0, 0]
            Intensity[:, :, i] = np.fft.fftshift(tiltImage_F / temp)
            step += 1
            self.progress.value = step

        #rescale intensity
        Intensity = np.power(Intensity, 0.2)

        #calculate variation itensity image
        Intensity_var = np.var(Intensity, axis=2)
        #search angles
        coarseStep = 2
        fineStep = 0.1
        coarseAngles = np.arange(-90, 90, coarseStep)

        Nx = Intensity_var.shape[0]
        Ny = Intensity_var.shape[1]
        N = np.round(np.min([Nx, Ny]) / 3)

        #coarse search
        I = np.zeros((coarseAngles.size, N))
        for a in range(coarseAngles.size):
            if self.canceled:
                return
            self.progress.message = ('Calculating line intensity at angle %f'
                                     'degree' % (coarseAngles[a]))
            I[a, :] = calculateLineIntensity(Intensity_var, coarseAngles[a], N)
            step += 1
            self.progress.value = step

        I_sum = np.sum(I, axis=1)
        minIntensityIndex = np.argmin(I_sum)
        rot_ang = coarseAngles[minIntensityIndex]

        #now refine
        fineAngles = np.arange(rot_ang - coarseStep,
                               rot_ang + coarseStep + fineStep, fineStep)

        #fine search
        I = np.zeros((fineAngles.size, N))
        for a in range(fineAngles.size):
            if self.canceled:
                return
            self.progress.message = ('Calculating line intensity at angle %f'
                                     'degree' % (fineAngles[a]))

            I[a, :] = calculateLineIntensity(Intensity_var, fineAngles[a], N)
            step += 1
            self.progress.value = step

        I_sum = np.sum(I, axis=1)
        minIntensityIndex = np.argmin(I_sum)
        rot_ang = coarseAngles[minIntensityIndex]

        self.progress.message = 'Rotating tilt series'
        result = ndimage.interpolation.rotate(
            tiltSeries, -rot_ang, axes=((0, 1)))
        #step += 1
        #self.progress.value = step
        print("rotate tilt series by %f degrees" % -rot_ang)

        # Set the result as the new scalars.
        utils.set_array(dataset, result)


def calculateLineIntensity(Intensity_var, angle_d, N):
    Nx = Intensity_var.shape[0]
    Ny = Intensity_var.shape[1]
    cenx = np.floor(Nx / 2)
    ceny = np.floor(Ny / 2)
    ang = angle_d * np.pi / 180
    w = np.zeros(N)
    v = np.zeros(N)
    for i in range(0, N):
        x = i * np.cos(ang)
        y = i * np.sin(ang)
        sx = abs(np.floor(x) - x)
        sy = abs(np.floor(y) - y)
        px = int(np.floor(x) + cenx)
        py = int(np.floor(y) + ceny)
        if (px >= 0 and px < Nx and py >= 0 and py < Ny):
            w[i] = w[i] + (1 - sx) * (1 - sy)
            v[i] = v[i] + (1 - sx) * (1 - sy) * Intensity_var[px, py]
        px = int(np.ceil(x) + cenx)
        py = int(np.floor(y) + ceny)
        if (px >= 0 and px < Nx and py >= 0 and py < Ny):
            w[i] = w[i] + sx * (1 - sy)
            v[i] = v[i] + sx * (1 - sy) * Intensity_var[px, py]
        px = int(np.floor(x) + cenx)
        py = int(np.ceil(y) + ceny)
        if (px >= 0 and px < Nx and py >= 0 and py < Ny):
            w[i] = w[i] + (1 - sx) * sy
            v[i] = v[i] + (1 - sx) * sy * Intensity_var[px, py]
        px = int(np.ceil(x) + cenx)
        py = int(np.ceil(y) + ceny)
        if (px >= 0 and px < Nx and py >= 0 and py < Ny):
            w[i] = w[i] + sx * sy
            v[i] = v[i] + sx * sy * Intensity_var[px, py]
    v[w != 0] = v[w != 0] / w[w != 0]
    return v

from tomviz import utils
import numpy as np
import tomviz.operators


class CrossCorrelationAlignmentOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """Automatically align tilt images by cross-correlation"""
        self.progress.maximum = 1

        tiltSeries = utils.get_array(dataset).astype(float)
        tiltAngles = utils.get_tilt_angles(dataset)

        # determine reference image index
        zeroDegreeTiltImage = None
        if tiltAngles is not None:
            zeroDegreeTiltImage = np.where(tiltAngles == 0)[0]

        if zeroDegreeTiltImage:
            referenceIndex = zeroDegreeTiltImage[0]
        else:
            referenceIndex = tiltSeries.shape[2] // 2

        # create Fourier space filter
        filterCutoff = 4
        (Ny, Nx, Nproj) = tiltSeries.shape
        ky = np.fft.fftfreq(Ny)
        kx = np.fft.fftfreq(Nx)
        [kX, kY] = np.meshgrid(kx, ky)
        kR = np.sqrt(kX**2 + kY**2)
        kFilter = (kR <= (0.5 / filterCutoff)) * \
            np.sin(2 * filterCutoff * np.pi * kR)**2

        # create real sapce filter to remove edge discontinuities
        y = np.linspace(1, Ny, Ny)
        x = np.linspace(1, Nx, Nx)
        [X, Y] = np.meshgrid(x, y)
        rFilter = (np.sin(np.pi * X / Nx) * np.sin(np.pi * Y / Ny)) ** 2

        self.progress.maximum = tiltSeries.shape[2] - 1
        step = 1

        offsets = np.zeros((tiltSeries.shape[2], 2))

        for i in range(referenceIndex, Nproj - 1):
            if self.canceled:
                return
            self.progress.message = 'Processing tilt image No.%d/%d' % (
                step, Nproj)

            offsets[i + 1, :], tiltSeries[:, :, i + 1] = crossCorrelationAlign(
                tiltSeries[:, :, i + 1], tiltSeries[:, :, i], rFilter, kFilter)
            step += 1
            self.progress.value = step

        for i in range(referenceIndex, 0, -1):
            if self.canceled:
                return
            self.progress.message = 'Processing tilt image No.%d/%d' % (
                step, Nproj)
            offsets[i - 1, :], tiltSeries[:, :, i - 1] = crossCorrelationAlign(
                tiltSeries[:, :, i - 1], tiltSeries[:, :, i], rFilter, kFilter)
            step += 1
            self.progress.value = step

        utils.set_array(dataset, tiltSeries)

        # Create a spreadsheet data set from table data
        column_names = ["X Offset", "Y Offset"]
        offsetsTable = utils.make_spreadsheet(column_names, offsets)
        # Set up dictionary to return operator results
        returnValues = {}
        returnValues["alignments"] = offsetsTable
        return returnValues


def crossCorrelationAlign(image, reference, rFilter, kFilter):
    """Align image to reference by cross-correlation"""

    image_f = np.fft.fft2((image - np.mean(image)) * rFilter)
    reference_f = np.fft.fft2((reference - np.mean(reference)) * rFilter)

    xcor = abs(np.fft.ifft2(np.conj(image_f) * reference_f * kFilter))
    shifts = np.unravel_index(xcor.argmax(), xcor.shape)

    # shift image
    output = np.roll(image, shifts[0], axis=0)
    output = np.roll(output, shifts[1], axis=1)

    return shifts, output

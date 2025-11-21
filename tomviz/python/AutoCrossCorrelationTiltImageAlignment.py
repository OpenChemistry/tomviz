from tomviz import utils
import tomviz.operators

from scipy import ndimage
import numpy as np


class CrossCorrelationAlignmentOperator(tomviz.operators.CancelableOperator):

    def transform(
        self,
        dataset,
        transform_source='generate',
        apply_to_all_arrays=True,
        transforms_save_file='',
        transform_file=None,
    ):
        kwargs = {
            'dataset': dataset,
            'apply_to_all_arrays': apply_to_all_arrays,
        }
        if transform_source == 'generate':
            kwargs = {
                **kwargs,
                'transforms_save_file': transforms_save_file,
            }
            return self.transform_generate(**kwargs)
        elif transform_source == 'from_file':
            kwargs = {
                **kwargs,
                'transform_file': transform_file,
            }
            return self.transform_from_file(**kwargs)

        raise NotImplementedError(transform_source)

    def transform_generate(self, dataset, apply_to_all_arrays,
                           transforms_save_file):
        """Automatically align tilt images by cross-correlation"""
        self.progress.maximum = 1

        tiltSeries = dataset.active_scalars.astype(float)
        tiltAngles = dataset.tilt_angles

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

        dataset.active_scalars = tiltSeries
        if apply_to_all_arrays:
            names = [name for name in dataset.scalars_names
                     if name != dataset.active_name]
            self.apply_offsets_to_scalar_names(dataset, offsets, names)

        if transforms_save_file:
            np.savez(
                transforms_save_file,
                offsets=offsets,
                spacing=dataset.spacing,
            )
            print('Saved transforms to:', transforms_save_file)

    def apply_offsets_to_scalar_names(self, dataset, offsets, names):
        # Now apply the same offsets to all other arrays
        for name in names:
            print(f'Applying shifts to {name}...')
            array = dataset.scalars(name)
            for i in range(len(offsets)):
                shifts = offsets[i]
                array[:, :, i] = ndimage.shift(array[:, :, i],
                                               shift=shifts,
                                               order=1,
                                               mode='wrap')

            dataset.set_scalars(name, array)

    def transform_from_file(self, dataset, apply_to_all_arrays,
                            transform_file):
        if not apply_to_all_arrays:
            names = [dataset.active_name]
        else:
            names = dataset.scalars_names

        with np.load(transform_file) as f:
            transform_offsets = f['offsets']
            transform_spacing = f['spacing']

        # The true shift will depend on the voxel size ratio, along
        # the shift direction, which is Y.
        offsets = transform_offsets.astype(float)
        offsets[:, 0] *= transform_spacing[0] / dataset.spacing[0]
        offsets[:, 1] *= transform_spacing[1] / dataset.spacing[1]
        return self.apply_offsets_to_scalar_names(dataset, offsets, names)


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

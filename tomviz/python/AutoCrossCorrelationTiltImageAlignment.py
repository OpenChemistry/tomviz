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
        referenceIndex = tiltSeries.shape[2] // 2
        if tiltAngles is not None:
            zeroDegreeTiltImage = np.where(tiltAngles == 0)[0]
            if zeroDegreeTiltImage.size > 0:
                referenceIndex = zeroDegreeTiltImage[0]

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

        if apply_to_all_arrays:
            names = dataset.scalars_names
        else:
            names = [dataset.active_name]
        pad_and_shift(dataset, offsets, names)

        if transforms_save_file:
            np.savez(
                transforms_save_file,
                offsets=offsets,
                spacing=dataset.spacing,
                signed=True,
            )
            print('Saved transforms to:', transforms_save_file)

    def transform_from_file(self, dataset, apply_to_all_arrays,
                            transform_file):
        if not apply_to_all_arrays:
            names = [dataset.active_name]
        else:
            names = dataset.scalars_names

        with np.load(transform_file) as f:
            transform_offsets = f['offsets']
            transform_spacing = f['spacing']
            signed = 'signed' in f

        # The true shift will depend on the voxel size ratio, along
        # the shift direction, which is Y.
        offsets = transform_offsets.astype(float)
        offsets[:, 0] *= transform_spacing[0] / dataset.spacing[0]
        offsets[:, 1] *= transform_spacing[1] / dataset.spacing[1]
        if not signed:
            # Files from earlier versions hold shifts as 0 to N - 1, which
            # only a wrap-around shift could apply
            size = np.array(dataset.active_scalars.shape[:2])
            offsets = (offsets + size / 2) % size - size / 2
        pad_and_shift(dataset, offsets, names)


def crossCorrelationAlign(image, reference, rFilter, kFilter):
    """Align image to reference by cross-correlation"""

    image_f = np.fft.fft2((image - np.mean(image)) * rFilter)
    reference_f = np.fft.fft2((reference - np.mean(reference)) * rFilter)

    xcor = abs(np.fft.ifft2(np.conj(image_f) * reference_f * kFilter))
    peak = np.unravel_index(xcor.argmax(), xcor.shape)
    # The peak is at 0 to N - 1; past the middle it is a negative shift
    shifts = [int(p) - n if p > n // 2 else int(p)
              for p, n in zip(peak, xcor.shape)]

    # shift image
    output = np.roll(image, shifts[0], axis=0)
    output = np.roll(output, shifts[1], axis=1)

    return shifts, output


def pad_and_shift(dataset, offsets: np.ndarray, names: list[str]) -> None:
    """Shift each image of the named arrays by its row of offsets.

    Every array is padded with zeros by the largest shift on both sides
    first, so nothing is shifted out of view or wraps around to the
    other side, and the image center stays put.
    """
    pad = np.ceil(np.abs(offsets).max(axis=0)).astype(int)
    for name in dataset.scalars_names:
        array = dataset.scalars(name)
        # Multi-component arrays have a fourth axis, left as it is
        rest = array.ndim - 3
        array = np.pad(array, ((pad[0], pad[0]), (pad[1], pad[1]), (0, 0)) +
                       ((0, 0),) * rest)
        if name in names:
            print(f'Applying shifts to {name}...')
            for i, shift in enumerate(offsets):
                array[:, :, i] = ndimage.shift(array[:, :, i],
                                               tuple(shift) + (0,) * rest,
                                               order=1, mode='constant')
        dataset.set_scalars(name, array)

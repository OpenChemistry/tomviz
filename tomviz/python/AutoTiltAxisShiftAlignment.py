import numpy as np
from scipy.interpolate import interp1d
from scipy import ndimage

import tomviz.operators
from tomviz.utils import pad_array


class AutoTiltAxisShiftAlignmentOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, transform_source: str = 'generate',
                  transform_file: str = '', padding: int = 0,
                  num_slices: int = 5, seed: int = 0,
                  apply_to_all_arrays: bool = True,
                  transforms_save_file: str = ''):
        """Automatic align the tilt axis to the center of images"""
        shared_kwargs = {
            'dataset': dataset,
            'apply_to_all_arrays': apply_to_all_arrays,
        }
        if transform_source == 'generate':
            kwargs = {
                **shared_kwargs,
                'padding': padding,
                'num_slices': num_slices,
                'seed': seed,
                'transforms_save_file': transforms_save_file,
            }
            f = self.transform_generate
        elif transform_source == 'from_file':
            kwargs = {
                **shared_kwargs,
                'transform_file': transform_file,
            }
            f = self.transform_from_file

        return f(**kwargs)

    def transform_generate(self, dataset, padding: int = 0,
                           num_slices: int = 5, seed: int = 0,
                           apply_to_all_arrays: bool = True,
                           transforms_save_file: str = ''):
        self.progress.maximum = 1

        # Get Tilt angles
        tilt_angles = dataset.tilt_angles

        tiltSeries = dataset.active_scalars
        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        axis = 2
        tiltSeries = pad_array(tiltSeries, padding, axis)

        Nx, Ny, Nz = tiltSeries.shape

        shifts = (np.linspace(-20, 20, 41)).astype('int')

        # randomly choose slices with top 50% total intensities
        tiltSeriesSum = np.sum(tiltSeries, axis=(1, 2))
        temp = tiltSeriesSum.argsort()[Nx // 2:]

        if num_slices > temp.size:
            print(f'Warning: number of slices selected, "{num_slices}", '
                  f'exceeded the max size of "{temp.size}". '
                  f'Reducing the number of slices to "{temp.size}"')
            num_slices = temp.size

        random_state = np.random.RandomState(seed=seed)
        slices = np.sort(temp[random_state.permutation(temp.size)[:num_slices]])
        print('Trial reconstruction slices:')
        print(slices)

        I = np.zeros(shifts.size)

        self.progress.maximum = shifts.size - 1
        step = 1

        for i in range(shifts.size):
            if self.canceled:
                return
            shiftedTiltSeries = np.roll(
                tiltSeries[slices, :, :, ], shifts[i], axis=1)
            for s in range(num_slices):
                self.progress.message = ('Reconstructing slice No.%d/%d with a '
                                         'shift of %d pixels' %
                                         (s, num_slices, shifts[i]))

                recon = wbp2(shiftedTiltSeries[s, :, :],
                             tilt_angles, Ny, 'ramp', 'linear')
                I[i] = I[i] + np.amax(recon)

            step += 1
            self.progress.value = step

        shift = shifts[np.argmax(I)]
        print(f'shift: {shift}')

        if transforms_save_file:
            np.savez(
                transforms_save_file,
                shift=shift,
                spacing=dataset.spacing,
            )
            print('Saved transforms file to:', transforms_save_file)

        return self.apply_shift_to_arrays(dataset, shift, apply_to_all_arrays)

    def transform_from_file(self, dataset, transform_file: str = '',
                            apply_to_all_arrays: bool = True):

        with np.load(transform_file) as f:
            transform_shift = f['shift']
            transform_spacing = f['spacing']

        # The true shift will depend on the voxel size ratio, along
        # the shift direction, which is Y.
        axis = 1
        shift = (
            transform_shift * transform_spacing[axis] / dataset.spacing[axis]
        )
        return self.apply_shift_to_arrays(dataset, shift, apply_to_all_arrays)

    def apply_shift_to_arrays(self, dataset, shift,
                              apply_to_all_arrays: bool = True):
        if apply_to_all_arrays:
            names = dataset.scalars_names
        else:
            names = [dataset.active_name]

        for name in names:
            array = dataset.scalars(name)
            result = ndimage.shift(array, shift=(0, shift, 0), order=1)
            result = np.asfortranarray(result)

            # Set the result as the new scalars.
            dataset.set_scalars(name, result)


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

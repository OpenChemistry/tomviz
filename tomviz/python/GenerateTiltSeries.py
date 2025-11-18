import numpy as np
import scipy.ndimage
import tomviz.operators


class GenerateTiltSeriesOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, start_angle=-90.0, angle_increment=3.0,
                  num_tilts=60):
        """Generate Tilt Series from Volume"""
        self.progress.maximum = 1

        self.progress.message = 'Initialization'
        # Generate Tilt Angles.
        angles = np.linspace(start_angle, start_angle +
                             (num_tilts - 1) * angle_increment, num_tilts)

        volume = dataset.active_scalars
        Ny = volume.shape[1]
        Nz = volume.shape[2]
        # calculate the size s.t. it contains the entire volume
        N = np.round(np.sqrt(Ny**2 + Nz**2))
        N = int(np.floor(N / 2.0) * 2 + 1)  # make the size an odd integer

        # pad volume
        pad_y_pre = int(np.ceil((N - Ny) / 2.0))
        pad_y_post = int(np.floor((N - Ny) / 2.0))
        pad_z_pre = int(np.ceil((N - Nz) / 2.0))
        pad_z_post = int(np.floor((N - Nz) / 2.0))
        volume_pad = np.pad(
            volume, ((0, 0), (pad_y_pre, pad_y_post), (pad_z_pre, pad_z_post)),
            'constant')

        Nslice = volume.shape[0]  # Number of slices along rotation axis.
        tiltSeries = np.empty([Nslice, N, num_tilts], dtype=float, order='F')

        self.progress.maximum = num_tilts
        step = 0

        for i in range(num_tilts):
            if self.canceled:
                return
            self.progress.message = 'Generating tilt image No.%d/%d' % (
                i + 1, num_tilts)

            # Rotate volume about x-axis
            rotatedVolume = np.empty_like(volume_pad)
            scipy.ndimage.interpolation.rotate(
                volume_pad, angles[i], axes=(1, 2), reshape=False, order=1,
                output=rotatedVolume)
            # Calculate projection
            tiltSeries[:, :, i] = np.sum(rotatedVolume, axis=2)

            step += 1
            self.progress.value = step

        # Set the result as the new scalars.
        dataset.active_scalars = tiltSeries

        # Save tilt angles.
        dataset.tilt_angles = angles

from tomviz import utils
import numpy as np
import scipy.ndimage
import tomviz.operators


class ReconARTOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """Generate Tilt Series from Volume"""
        self.progress.maximum = 1

        #----USER SPECIFIED VARIABLES-----#
        ###start_angle###    #Starting angle
        ###angle_increment###   #Angle increment
        ###num_tilts### #Number of tilts
        #---------------------------------#

        # Generate Tilt Angles.
        angles = np.linspace(start_angle, start_angle +
                             (num_tilts - 1) * angle_increment, num_tilts)

        volume = utils.get_array(dataset)
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
        volume_pad = np.lib.pad(
            volume, ((0, 0), (pad_y_pre, pad_y_post), (pad_z_pre, pad_z_post)),
            'constant')

        Nslice = volume.shape[0]  # Number of slices along rotation axis.
        tiltSeries = np.zeros((Nslice, N, num_tilts))

        self.progress.maximum = num_tilts
        step = 0

        for i in range(num_tilts):
            if self.canceled:
                return

            # Rotate volume about x-axis
            rotatedVolume = scipy.ndimage.interpolation.rotate(
                volume_pad, angles[i], axes=(1, 2), reshape=False, order=1)
            print rotatedVolume.shape
            # Calculate projection
            tiltSeries[:, :, i] = np.sum(rotatedVolume, axis=2)

            step += 1
            self.progress.update(step)

        # Set the result as the new scalars.
        utils.set_array(dataset, tiltSeries)

        # Mark dataset as tilt series.
        utils.mark_as_tiltseries(dataset)

        # Save tilt angles.
        utils.set_tilt_angles(dataset, angles)

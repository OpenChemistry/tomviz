from tomviz import utils
import numpy as np
import tomviz.operators


class CenterOfMassAlignmentOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """Automatically align tilt images by center of mass method"""
        self.progress.maximum = 1

        tiltSeries = utils.get_array(dataset).astype(float)
        tiltAngles = utils.get_tilt_angles(dataset)

        self.progress.maximum = tiltSeries.shape[2]
        step = 0

        for i in range(tiltSeries.shape[2]):
            if self.canceled:
                return
            tiltSeries[:, :, i] = centerOfMassAlign(tiltSeries[:, :, i])

            step += 1
            self.progress.update(step)

        utils.set_array(dataset, tiltSeries)


def centerOfMassAlign(image):
    """Shift image so that the center of mass of is at origin"""
    (Nx, Ny) = image.shape
    # set up coordinate
    y = np.linspace(0, Ny - 1, Ny)
    x = np.linspace(0, Nx - 1, Nx)
    [X, Y] = np.meshgrid(y, x)

    imageCOM_x = int(np.sum(image * X) / np.sum(image))
    imageCOM_y = int(np.sum(image * Y) / np.sum(image))

    sx = -(imageCOM_x - Nx // 2)
    sy = -(imageCOM_y - Ny // 2)

    output = np.roll(image,  sx, axis=1)
    output = np.roll(output, sy, axis=0)

    return output

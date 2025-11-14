from tomviz import utils
import numpy as np
import tomviz.operators


class CenterOfMassAlignmentOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        """Automatically align tilt images by center of mass method"""
        self.progress.maximum = 1

        tiltSeries = dataset.active_scalars.astype(float)

        self.progress.maximum = tiltSeries.shape[2]
        step = 1

        offsets = np.zeros((tiltSeries.shape[2], 2))

        for i in range(tiltSeries.shape[2]):
            if self.canceled:
                return
            self.progress.message = 'Processing tilt image No.%d/%d' % (
                i + 1, tiltSeries.shape[2])

            offsets[i, :], tiltSeries[:, :, i] = centerOfMassAlign(
                tiltSeries[:, :, i]
            )

            step += 1
            self.progress.value = step

        dataset.active_scalars = tiltSeries

        # Create a spreadsheet data set from table data
        column_names = ["X Offset", "Y Offset"]
        offsetsTable = utils.make_spreadsheet(column_names, offsets)
        # Set up dictionary to return operator results
        returnValues = {}
        returnValues["alignments"] = offsetsTable
        return returnValues


def centerOfMassAlign(image):
    """Shift image so that the center of mass of is at origin"""
    (Nx, Ny) = image.shape
    # set up coordinate
    y = np.linspace(0, Ny - 1, Ny)
    x = np.linspace(0, Nx - 1, Nx)
    [X, Y] = np.meshgrid(x, y, indexing="ij")

    imageCOM_x = int(np.sum(image * X) / np.sum(image))
    imageCOM_y = int(np.sum(image * Y) / np.sum(image))

    sx = -(imageCOM_x - Nx // 2)
    sy = -(imageCOM_y - Ny // 2)

    output = np.roll(image, sx, axis=0)
    output = np.roll(output, sy, axis=1)

    return (sx, sy), output

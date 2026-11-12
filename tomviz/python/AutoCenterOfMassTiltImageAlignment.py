from tomviz import utils
import tomviz.operators

import numpy as np
from scipy import ndimage


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

            offsets[i, :] = centerOfMassAlign(tiltSeries[:, :, i])

            step += 1
            self.progress.value = step

        pad_and_shift(dataset, offsets, dataset.scalars_names)

        # Create a spreadsheet data set from table data
        column_names = ["X Offset", "Y Offset"]
        offsetsTable = utils.make_spreadsheet(column_names, offsets)
        # Set up dictionary to return operator results
        returnValues = {}
        returnValues["alignments"] = offsetsTable
        return returnValues


def centerOfMassAlign(image):
    """The shift that moves the image's center of mass to its middle"""
    (Nx, Ny) = image.shape
    # set up coordinate
    y = np.linspace(0, Ny - 1, Ny)
    x = np.linspace(0, Nx - 1, Nx)
    [X, Y] = np.meshgrid(x, y, indexing="ij")

    imageCOM_x = int(np.sum(image * X) / np.sum(image))
    imageCOM_y = int(np.sum(image * Y) / np.sum(image))

    sx = -(imageCOM_x - Nx // 2)
    sy = -(imageCOM_y - Ny // 2)

    return sx, sy


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

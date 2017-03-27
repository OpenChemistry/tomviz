import numpy as np
from tomviz import utils
import tomviz.operators


class AddPoissonNoiseOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset, N=25):
        """Add Poisson noise to tilt images"""
        self.progress.maximum = 1

        tiltSeries = utils.get_array(dataset).astype(float)
        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        Ndata = tiltSeries.shape[0] * tiltSeries.shape[1]

        self.progress.maximum = tiltSeries.shape[2]
        step = 0
        for i in range(tiltSeries.shape[2]):
            if self.canceled:
                return

            tiltImage = tiltSeries[:, :, i].copy()
            tiltImage = tiltImage / np.sum(tiltSeries[:, :, i]) * (Ndata * N)
            tiltImage = np.random.poisson(tiltImage)
            tiltImage = tiltImage * np.sum(tiltSeries[:, :, i]) / (Ndata * N)

            #calculate signal-to-noise ratio
            snr = np.mean(tiltSeries[:, :, i]) / \
                np.std(tiltSeries[:, :, i] - tiltImage)
            tiltSeries[:, :, i] = tiltImage.copy()
            step += 1
            self.progress.value = step

        utils.set_array(dataset, tiltSeries)

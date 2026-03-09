import numpy as np
import tomviz.operators


class AddPoissonNoiseOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, N=25):
        """Add Poisson noise to tilt images"""
        self.progress.maximum = 1

        tiltSeries = dataset.active_scalars.astype(float)
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

            tiltSeries[:, :, i] = tiltImage.copy()
            step += 1
            self.progress.value = step

        dataset.active_scalars = tiltSeries

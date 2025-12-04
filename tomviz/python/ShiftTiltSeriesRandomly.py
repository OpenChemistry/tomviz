import numpy as np
import tomviz.operators


class RandomTiltSeriesShiftOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset, maxShift=1):
        """Apply random integer shifts to tilt images"""

        tiltSeries = dataset.active_scalars
        if tiltSeries is None:
            raise RuntimeError("No scalars found!")

        self.progress.maximum = tiltSeries.shape[2]
        arrays = {k: dataset.scalars(k) for k in dataset.scalars_names}
        step = 0
        for i in range(tiltSeries.shape[2]):
            if self.canceled:
                return
            shifts = (np.random.rand(2) * 2 - 1) * maxShift
            for array in arrays.values():
                array[:, :, i] = np.roll(
                    array[:, :, i], int(shifts[0]), axis=0)
                array[:, :, i] = np.roll(
                    array[:, :, i], int(shifts[1]), axis=1)
            step += 1
            self.progress.value = step

        for name, array in arrays.items():
            dataset.set_scalars(name, array)

import tomviz.operators

NUMBER_OF_CHUNKS = 10


class InvertOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        import numpy as np
        self.progress.maximum = NUMBER_OF_CHUNKS

        scalars = dataset.active_scalars
        if scalars is None:
            raise RuntimeError("No scalars found!")

        result = np.float32(scalars)
        min = np.amin(scalars)
        max = np.amax(scalars)
        step = 0
        for chunk in np.array_split(result, NUMBER_OF_CHUNKS):
            if self.canceled:
                return
            chunk[:] = max - chunk + min
            step += 1
            self.progress.value = step

        dataset.active_scalars = result

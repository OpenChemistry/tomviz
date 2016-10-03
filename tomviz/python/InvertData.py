import tomviz.operators

NUMBER_OF_CHUNKS = 10


class InvertOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        from tomviz import utils
        import numpy as np

        scalars = utils.get_scalars(dataset)
        if scalars is None:
            raise RuntimeError("No scalars found!")

        result = np.float32(scalars)
        max = np.amax(scalars)
        for chunk in np.array_split(result, NUMBER_OF_CHUNKS):
            if self.canceled:
                return
            chunk[:] = max - chunk

        utils.set_scalars(dataset, result)

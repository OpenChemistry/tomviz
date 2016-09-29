import tomviz.operators

NUMBER_OF_CHUNKS = 10


class SquareRootOperator(tomviz.operators.CancelableOperator):
    def transform_scalars(self, dataset):
        """Define this method for Python operators that
        transform input scalars"""

        from tomviz import utils
        import numpy as np

        scalars = utils.get_scalars(dataset)
        if scalars is None:
            raise RuntimeError("No scalars found!")

        if scalars.min() < 0:
            print("WARNING: Square root of negative values results in NaN!")
        else:
            # transform the dataset
            # Process dataset in chunks so the user gets an opportunity to
            # cancel.
            result = np.float32(scalars)
            for chunk in np.array_split(result, NUMBER_OF_CHUNKS):
                if self.canceled:
                    return
                np.sqrt(chunk, chunk)

            # set the result as the new scalars.
            utils.set_scalars(dataset, result)

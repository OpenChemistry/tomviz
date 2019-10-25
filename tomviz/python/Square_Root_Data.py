import tomviz.operators

NUMBER_OF_CHUNKS = 10


class SquareRootOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        """Define this method for Python operators that
        transform input scalars"""

        import numpy as np
        self.progress.maximum = NUMBER_OF_CHUNKS

        scalars = dataset.active_scalars
        if scalars is None:
            raise RuntimeError("No scalars found!")

        if scalars.min() < 0:
            print("WARNING: Square root of negative values results in NaN!")
        else:
            # transform the dataset
            # Process dataset in chunks so the user gets an opportunity to
            # cancel.
            result = np.float32(scalars)
            step = 0
            for chunk in np.array_split(result, NUMBER_OF_CHUNKS):
                if self.canceled:
                    return
                np.sqrt(chunk, chunk)
                step += 1
                self.progress.value = step

            # set the result as the new scalars.
            dataset.active_scalars = result

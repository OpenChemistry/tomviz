def transform_scalars(dataset):
    from tomviz import utils
    import numpy as np

    scalars = utils.get_scalars(dataset)
    if scalars is None:
        raise RuntimeError("No scalars found!")

    result = np.amax(scalars) - scalars
    utils.set_scalars(dataset, result)

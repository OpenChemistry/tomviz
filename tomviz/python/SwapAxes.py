from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, axis1, axis2):
    """Swap two axes in a dataset"""

    import numpy as np

    data_py = dataset.active_scalars
    swapped = data_py.swapaxes(axis1, axis2)
    dataset.active_scalars = np.asfortranarray(swapped)

def transform(dataset, axis1, axis2):
    """Swap two axes in a dataset"""

    data_py = dataset.active_scalars
    dataset.active_scalars = data_py.swapaxes(axis1, axis2)

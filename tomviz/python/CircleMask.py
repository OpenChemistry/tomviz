from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, axis, ratio, value):
    try:
        import tomopy
    except ImportError:
        print('Failed to import tomopy, which is required for this operator')
        raise

    array = dataset.active_scalars

    result = tomopy.misc.corr.circ_mask(array, axis, ratio, value)

    dataset.active_scalars = result

def transform_scalars(dataset):
    """Set negative voxels to zero"""

    from tomviz import utils

    data = utils.get_array(dataset)

    data[data<0] = 0 # Set negative voxels to zero

    # Set the result as the new scalars.
    utils.set_array(dataset, data)

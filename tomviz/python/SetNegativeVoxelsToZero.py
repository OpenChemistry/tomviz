def transform(dataset):
    """Set negative voxels to zero"""

    data = dataset.active_scalars

    data[data < 0] = 0 # Set negative voxels to zero

    # Set the result as the new scalars.
    dataset.active_scalars = data

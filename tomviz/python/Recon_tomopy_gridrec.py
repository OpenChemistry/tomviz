def transform_scalars(dataset, rot_center=0, tune_rot_center=True):
    """Reconstruct sinograms using the tomopy gridrec algorithm

    Typically, a data exchange file would be loaded for this
    reconstruction. This operation will attempt to perform
    flat-field correction of the raw data using the dark and
    white background data found in the data exchange file.

    This operator also requires either the tomviz/tomopy-pipeline
    docker image, or a python environment with tomopy installed.
    """

    from tomviz import utils
    import numpy as np
    import tomopy

    # Get the current volume as a numpy array.
    array = utils.get_array(dataset)

    dark = dataset.dark
    white = dataset.white

    # No current way to get theta
    theta = None

    if theta is None:
        # Assume it is equally spaced between 0 and 180 degrees
        theta = tomopy.angles(array.shape[0])

    # Perform flat-field correction of raw data
    if white is not None and dark is not None:
        array = tomopy.normalize(array, white, dark, cutoff=1.4)

    if rot_center == 0:
        # Try to find it automatically
        init = array.shape[2] / 2.0
        rot_center = tomopy.find_center(array, theta, init=init, ind=0,
                                        tol=0.5)
    elif tune_rot_center:
        # Tune the center
        rot_center = tomopy.find_center(array, theta, init=rot_center, ind=0,
                                        tol=0.5)

    # Calculate -log(array)
    array = tomopy.minus_log(array)

    # Perform the reconstruction
    array = tomopy.recon(array, theta, center=rot_center, algorithm='gridrec')

    # Mask each reconstructed slice with a circle.
    array = tomopy.circ_mask(array, axis=0, ratio=0.95)

    # Set the transformed array
    utils.set_array(dataset, array)

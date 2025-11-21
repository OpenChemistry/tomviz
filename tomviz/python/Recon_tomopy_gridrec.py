from tomviz.utils import apply_to_each_array


@apply_to_each_array
def transform(dataset, rot_center=0, tune_rot_center=True):
    """Reconstruct sinograms using the tomopy gridrec algorithm

    Typically, a data exchange file would be loaded for this
    reconstruction. This operation will attempt to perform
    flat-field correction of the raw data using the dark and
    white background data found in the data exchange file.

    This operator also requires either the tomviz/tomopy-pipeline
    docker image, or a python environment with tomopy installed.
    """

    import numpy as np
    import tomopy

    # Get the current volume as a numpy array.
    array = dataset.active_scalars

    dark = dataset.dark
    white = dataset.white
    angles = dataset.tilt_angles
    tilt_axis = dataset.tilt_axis

    # TomoPy wants the tilt axis to be zero, so ensure that is true
    if tilt_axis == 2:
        order = [2, 1, 0]
        array = np.transpose(array, order)
        if dark is not None and white is not None:
            dark = np.transpose(dark, order)
            white = np.transpose(white, order)

    if angles is not None:
        # tomopy wants radians
        theta = np.radians(angles)
    else:
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

    # Remove nan, neg, and inf values
    array = tomopy.remove_nan(array, val=0.0)
    array = tomopy.remove_neg(array, val=0.00)
    array[np.where(array == np.inf)] = 0.00

    # Perform the reconstruction
    array = tomopy.recon(array, theta, center=rot_center, algorithm='gridrec')

    # Mask each reconstructed slice with a circle.
    array = tomopy.circ_mask(array, axis=0, ratio=0.95)

    # Set the transformed array
    child = dataset.create_child_dataset()
    child.active_scalars = array

    return_values = {}
    return_values['reconstruction'] = child
    return return_values

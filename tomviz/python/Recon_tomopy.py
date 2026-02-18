import numpy as np
import tomopy


def transform(dataset, algorithm='gridrec', num_iter=5):
    data = dataset.active_scalars
    tilt_axis = dataset.tilt_axis

    # TomoPy wants the tilt axis to be zero, so ensure that is true
    if tilt_axis == 2:
        data = np.transpose(data, (2, 1, 0))

    # Normalize to [0, 1]
    data = data.astype(np.float32)
    data = (data - data.min()) / (data.max() - data.min())

    angles_rad = np.deg2rad(dataset.tilt_angles)
    center = data.shape[2] // 2

    # Reconstruct
    recon_kwargs = {}
    if algorithm == 'mlem':
        recon_kwargs['num_iter'] = num_iter

    rec = tomopy.recon(data, angles_rad, center=center, algorithm=algorithm,
                       **recon_kwargs)

    # Apply circular mask
    rec = tomopy.circ_mask(rec, axis=0, ratio=0.95, val=0.0)

    # Transpose back to expected Tomviz format
    rec = np.transpose(rec, (2, 0, 1))

    child = dataset.create_child_dataset()
    child.active_scalars = rec

    return {'reconstruction': child}

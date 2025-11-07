import h5py
from pathlib import Path

from scipy.ndimage.interpolation import rotate


def list_elements(filename):
    with h5py.File(filename, 'r') as f:
        elements = f['/reconstruction/fitting/elements'][()]

    return [x.decode() for x in elements]


def extract_elements(filename, elements, output_path, rotate_datasets):
    output_path = Path(output_path)
    output_path.mkdir(parents=True, exist_ok=True)

    all_elements = list_elements(filename)
    keep_indices = [all_elements.index(x) for x in elements]

    with h5py.File(filename, 'r') as f:
        angles = f['/exchange/theta'][()]
        recon = f['/reconstruction/fitting/data'][:, keep_indices]

    ret = []
    for i, element in enumerate(elements):
        this_recon = recon[:, i]
        if rotate_datasets:
            this_recon = rotate(this_recon, -90.0, axes=(1, 2))

        file_path = f'{output_path / element}.h5'
        with h5py.File(file_path, 'w') as wf:
            wf['/exchange/theta'] = angles
            wf['/exchange/data'] = this_recon

        ret.append(file_path)

    return ret

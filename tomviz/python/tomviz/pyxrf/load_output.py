import h5py
from pathlib import Path

from scipy.ndimage.interpolation import rotate

from tomviz.executor import _write_emd
from tomviz.external_dataset import Dataset


def list_elements(filename):
    with h5py.File(filename, 'r') as f:
        elements = f['/reconstruction/fitting/elements'][()]

    return [x.decode() for x in elements]


def extract_elements(filename, elements, output_path, rotate_datasets,
                     pixel_size_x, pixel_size_y):
    output_path = Path(output_path)
    output_path.mkdir(parents=True, exist_ok=True)

    all_elements = list_elements(filename)
    keep_indices = [all_elements.index(x) for x in elements]

    has_pixel_sizes = pixel_size_x > 0 and pixel_size_y > 0

    with h5py.File(filename, 'r') as f:
        angles = f['/exchange/theta'][()]
        recon = f['/reconstruction/fitting/data'][:, keep_indices]

    ret = []
    for i, element in enumerate(elements):
        this_recon = recon[:, i]
        if rotate_datasets:
            this_recon = rotate(this_recon, -90.0, axes=(1, 2))

        dataset = Dataset({element: this_recon.swapaxes(0, 2)})
        dataset.active_name = element
        dataset.tilt_angles = angles
        dataset.tilt_axis = 2
        if has_pixel_sizes:
            # Also set the pixel sizes if they are available
            dataset.spacing = (pixel_size_x, pixel_size_y, 1)

        file_path = f'{output_path / element}.emd'
        _write_emd(file_path, dataset)
        ret.append(file_path)

    return ret

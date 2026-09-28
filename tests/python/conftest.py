from pathlib import Path

import numpy as np
import pytest

from tomviz_pipeline.io.emd import load_dataset
from tomviz_pipeline.dataset import Dataset, LegacyDataset

from utils import download_file, download_and_unzip_file

DATA_URL = 'https://data.kitware.com/api/v1/file'


@pytest.fixture
def data_dir() -> Path:
    return Path(__file__).parent / 'data'


@pytest.fixture
def hxn_xrf_example_output_dir(data_dir: Path) -> Path:
    output_dir = data_dir / 'Pt_Zn_XRF_recon_output'
    if not output_dir.exists():
        # Download it
        url = DATA_URL + '/6914b90283abdcd84d150c9e/download'
        download_and_unzip_file(url, output_dir.parent)

    return output_dir


@pytest.fixture(scope='function')
def hxn_xrf_example_dataset(hxn_xrf_example_output_dir: Path) -> LegacyDataset:
    example_files = [
        'Pt_L.h5',
        'Zn_K.h5',
    ]
    example_files = [
        hxn_xrf_example_output_dir / 'extracted_elements' / name
        for name in example_files
    ]
    dataset = load_dataset(example_files[0])
    for new_file in example_files[1:]:
        new_dataset = load_dataset(new_file)
        new_name = new_dataset.active_name
        dataset.arrays[new_name] = new_dataset.active_scalars

    return LegacyDataset.from_dataset(dataset)


@pytest.fixture
def chipset_test_data_dir(data_dir: Path) -> Path:
    output_dir = data_dir / 'chipset_test_data'
    if not output_dir.exists():
        # Download it
        url = DATA_URL + '/69a5e30a90b2fab670f34788/download'
        download_and_unzip_file(url, output_dir.parent)

    return output_dir


@pytest.fixture(scope='function')
def chipset_xrf_dataset(chipset_test_data_dir: Path) -> Dataset:
    return load_dataset(chipset_test_data_dir / 'xrf_extracted_elements.emd')


@pytest.fixture(scope='function')
def chipset_ptycho_dataset(chipset_test_data_dir: Path) -> Dataset:
    return load_dataset(chipset_test_data_dir / 'ptycho_object.emd')


@pytest.fixture(scope='function')
def chipset_probe_dataset(chipset_test_data_dir: Path) -> Dataset:
    return load_dataset(chipset_test_data_dir / 'ptycho_probe.emd')


@pytest.fixture
def pystackreg_reference_output(data_dir: Path) -> dict[str, np.ndarray]:
    filepath = data_dir / 'test_pystackreg_reference_output.npz'
    if not filepath.exists():
        # Download it
        url = DATA_URL + '/690e69aa83abdcd84d150c7e/download'
        download_file(url, filepath)

    return np.load(filepath)


@pytest.fixture
def xcorr_reference_output(data_dir: Path) -> dict[str, np.ndarray]:
    filepath = data_dir / 'test_xcorr_reference_output.npz'
    if not filepath.exists():
        # Download it
        url = DATA_URL + '/6920b212cdb169d7a021d769/download'
        download_file(url, filepath)

    return np.load(filepath)


@pytest.fixture
def tilt_axis_shift_reference_output(data_dir: Path) -> dict[str, np.ndarray]:
    filepath = data_dir / 'test_tilt_axis_shift_reference_output.npz'
    if not filepath.exists():
        # Download it
        url = DATA_URL + '/6920b333cdb169d7a021d76c/download'
        download_file(url, filepath)

    return np.load(filepath)


@pytest.fixture
def constraint_dft_reference_output(data_dir: Path) -> dict[str, np.ndarray]:
    filepath = data_dir / 'test_constraint_dft_reference_output.npz'
    if not filepath.exists():
        # Download it
        url = DATA_URL + '/6920b750cdb169d7a021d773/download'
        download_file(url, filepath)

    return np.load(filepath)



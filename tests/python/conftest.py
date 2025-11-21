import pytest
import requests
import diskcache
import tempfile
import os
from pathlib import Path
import tarfile
import shutil

import numpy as np

from tomviz.executor import load_dataset
from tomviz.external_dataset import Dataset

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
def hxn_xrf_example_dataset(hxn_xrf_example_output_dir: Path) -> Dataset:
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

    return dataset


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


@pytest.fixture(scope="module")
def test_state_file(tmpdir_factory):
    tmpdir = tmpdir_factory.mktemp('state')

    _id = '5dbca381e3566bda4b4f94f0'
    download_url = '%s/%s/download' % (DATA_URL, _id)
    cache_path = os.path.join(tempfile.gettempdir(), 'tomviz_test_cache')
    with diskcache.Cache(cache_path) as cache:
        state_file_key = '%s#%s' % (download_url, 'state.tvsm')
        if download_url not in cache:
            response = requests.get(download_url, stream=True)
            response.raise_for_status()

            with tempfile.TemporaryFile() as fp:
                for chunk in response.iter_content(chunk_size=1024):
                    if chunk:
                        fp.write(chunk)

                fp.seek(0)

                with tarfile.open(fileobj=fp, mode="r:gz") as tar:
                    for member in tar.getmembers():
                        member_fp = tar.extractfile(member)
                        key = '%s#%s' % (download_url, member.name)
                        cache.set(key, member_fp, read=True)

        state_file_path = tmpdir.join('state.tvsm')
        with open(state_file_path.strpath, 'wb') as state_fp:
            shutil.copyfileobj(cache.get(state_file_key, read=True), state_fp)

        data_file_key = '%s#%s' % (download_url, 'small.emd')
        data_file_path = tmpdir.join('small.emd')
        with open(data_file_path.strpath, 'wb') as data_fp:
            shutil.copyfileobj(cache.get(data_file_key, read=True), data_fp)

        yield state_file_path

        tmpdir.remove()

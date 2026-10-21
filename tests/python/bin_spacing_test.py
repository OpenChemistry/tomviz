import numpy as np

from utils import load_operator_module

from tomviz_pipeline.dataset import Dataset


def _make_dataset() -> Dataset:
    dataset = Dataset({'data': np.zeros((10, 12, 8))})
    dataset.spacing = [0.5, 0.25, 2.0]
    return dataset


def test_bin_tilt_series_doubles_pixel_size() -> None:
    dataset = _make_dataset()
    load_operator_module('BinTiltSeriesByTwo').transform(dataset)
    # The tilt axis is not binned
    assert np.allclose(dataset.spacing, [1.0, 0.5, 2.0])


def test_bin_volume_doubles_voxel_size() -> None:
    dataset = _make_dataset()
    load_operator_module('BinVolumeByTwo').transform(dataset)
    assert np.allclose(dataset.spacing, [1.0, 0.5, 4.0])

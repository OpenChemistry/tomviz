import numpy as np

from utils import load_operator_module

from tomviz_pipeline.dataset import Dataset


def test_binning_doubles_the_spacing_of_binned_axes() -> None:
    # A tilt series is not binned along its tilt axis
    for name, expected in (('BinTiltSeriesByTwo', [1.0, 0.5, 2.0]),
                           ('BinVolumeByTwo', [1.0, 0.5, 4.0])):
        dataset = Dataset({'data': np.zeros((10, 12, 8))})
        dataset.spacing = [0.5, 0.25, 2.0]
        load_operator_module(name).transform(dataset)
        assert np.allclose(dataset.spacing, expected), name

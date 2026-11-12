import numpy as np

from utils import load_operator_class, load_operator_module

from tomviz_pipeline.dataset import Dataset


def _crop(image: np.ndarray) -> np.ndarray:
    rows = np.nonzero(image.any(axis=1))[0]
    cols = np.nonzero(image.any(axis=0))[0]
    return image[rows[0]:rows[-1] + 1, cols[0]:cols[-1] + 1]


def test_center_of_mass_keeps_a_sample_whole() -> None:
    # Mostly near the top, with a faint part touching the bottom edge
    image = np.zeros((40, 30))
    image[2:7, 10:20] = 10.0
    image[36:, 10:20] = 1.0
    dataset = Dataset({'data': image[:, :, None]}, 'data')
    dataset.spacing = (1.0, 1.0, 1.0)

    module = load_operator_module('AutoCenterOfMassTiltImageAlignment')
    load_operator_class(module).transform(dataset)

    # Shifted down 14 rows and right 1 column, into padding instead of
    # wrapping the faint part around to the top
    out = dataset.active_scalars[:, :, 0]
    assert out.shape == (40 + 2 * 14, 30 + 2 * 1)
    assert np.array_equal(_crop(out), _crop(image))

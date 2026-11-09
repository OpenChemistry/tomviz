import numpy as np

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _run(name: str, arr: np.ndarray, **arguments) -> np.ndarray:
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / f'{name}.json').read_text(),
        'script': (OPERATOR_PATH / f'{name}.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    result = t.transform({'volume': PortData(ds, 'ImageData')})
    return result[t.output_ports()[0].name].payload.active_scalars


def test_remove_bad_pixels_replaces_only_the_spike() -> None:
    rng = np.random.default_rng(0)
    for image in (
        # Integer counts whose neighbors often agree
        np.round(rng.normal(100.0, 0.3, (32, 32))),
        # A weak XRF map: mostly zeros with lone small counts
        rng.poisson(0.05, (64, 64)).astype(float),
    ):
        # One inside and one on the edge
        spikes = [(20, 20), (0, 20)]
        for p in spikes:
            image[p] = 1000.0
        out = _run('RemoveBadPixelsTiltSeries', image[:, :, None])[:, :, 0]
        for p in spikes:
            assert out[p] < 200.0
            image[p] = out[p]
        assert np.count_nonzero(out != image) <= 5


def test_remove_bad_pixels_replaces_touching_spikes() -> None:
    rng = np.random.default_rng(0)
    for image in (
        rng.normal(100.0, 1.0, (32, 32)).astype(np.float32),
        np.round(rng.normal(100.0, 0.3, (32, 32))),
        rng.poisson(0.05, (64, 64)).astype(float),
    ):
        # Two bad pixels side by side, and two touching at a corner
        spikes = [(10, 10), (10, 11), (20, 20), (21, 21)]
        clean = image.copy()
        for p in spikes:
            image[p] = 1000.0
        # A thin bright line, whose end pixels look like one of a pair
        image[5, 20:26] += 1000.0
        out = _run('RemoveBadPixelsTiltSeries', image[:, :, None])[:, :, 0]
        for p in spikes:
            assert abs(out[p] - np.median(clean[p[0] - 1:p[0] + 2,
                                                p[1] - 1:p[1] + 2])) < 3.0
            image[p] = out[p]
        assert np.array_equal(out[5, 20:26], image[5, 20:26])
        assert np.count_nonzero(out != image) <= 5


def test_manual_background_subtracts_the_region_mean() -> None:
    arr = np.ones((8, 8, 3)) * np.array([1.0, 2.0, 3.0])
    out = _run('Subtract_TiltSer_Background', arr, XRANGE=[0, 2],
               YRANGE=[0, 2], ZRANGE=[0, 3])
    assert np.allclose(out, 0.0)

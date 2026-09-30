import numpy as np

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _run(name, arr, **arguments):
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / f'{name}.json').read_text(),
        'script': (OPERATOR_PATH / f'{name}.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    result = t.transform({'volume': PortData(ds, 'ImageData')})
    if not result:
        return None  # the operator raised
    return result[t.output_ports()[0].name].payload.active_scalars


def test_remove_bad_pixels_uses_its_declared_threshold():
    rng = np.random.default_rng(0)
    arr = rng.normal(10.0, 0.1, (16, 16, 2))
    arr[5, 7, 1] = 1000.0
    out = _run('RemoveBadPixelsTiltSeries', arr)
    assert abs(out[5, 7, 1] - 10.0) < 1.0
    assert np.allclose(out[:, :, 0], arr[:, :, 0], atol=1.0)


def test_manual_background_subtracts_the_region_mean():
    arr = np.ones((8, 8, 3)) * np.array([1.0, 2.0, 3.0])
    out = _run('Subtract_TiltSer_Background', arr, XRANGE=[0, 2],
               YRANGE=[0, 2], ZRANGE=[0, 3])
    assert np.allclose(out, 0.0)
    # The declared [0, 0] defaults select nothing
    assert _run('Subtract_TiltSer_Background', arr, XRANGE=[0, 0],
                YRANGE=[0, 0], ZRANGE=[0, 0]) is None

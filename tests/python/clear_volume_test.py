import numpy as np

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _run_clear_volume(arr, **arguments):
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / 'ClearVolume.json').read_text(),
        'script': (OPERATOR_PATH / 'ClearVolume.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    result = t.transform({'volume': PortData(ds, 'ImageData')})
    if not result:
        return None  # the operator raised
    return result[t.output_ports()[0].name].payload.active_scalars


def test_clear_volume_fills_the_box_and_keeps_the_shape():
    arr = np.ones((6, 5, 4))
    out = _run_clear_volume(arr, XRANGE=[1, 3], YRANGE=[0, 5],
                            ZRANGE=[2, 4], fill_value=-1.0)
    assert out.shape == arr.shape
    assert np.all(out[1:3, :, 2:4] == -1.0)
    assert np.all(out[0] == 1.0) and np.all(out[3:] == 1.0)
    assert np.all(out[1:3, :, :2] == 1.0)


def test_clear_volume_needs_a_region():
    # The declared [0, 0] defaults select nothing
    assert _run_clear_volume(np.ones((6, 5, 4)), XRANGE=[0, 0],
                             YRANGE=[0, 0], ZRANGE=[0, 0]) is None

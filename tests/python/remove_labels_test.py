import numpy as np
import pytest

from tomviz_pipeline import PortData
from tomviz_pipeline.dataset import Dataset
from tomviz_pipeline.nodes.transforms.legacy_python import (
    LegacyPythonTransform,
)

from utils import OPERATOR_PATH


def _remove(arr, **arguments):
    t = LegacyPythonTransform()
    t.deserialize({
        'description': (OPERATOR_PATH / 'RemoveLabels.json').read_text(),
        'script': (OPERATOR_PATH / 'RemoveLabels.py').read_text(),
        'arguments': arguments,
    })
    ds = Dataset({'ImageScalars': np.asfortranarray(arr)}, 'ImageScalars')
    ds.spacing = (1.0, 1.0, 1.0)
    result = t.transform({'volume': PortData(ds, 'LabelMap')})
    return result[t.output_ports()[0].name].payload.active_scalars


def _labels():
    # Labels 0..7, one per z slice
    return np.broadcast_to(np.arange(8, dtype=np.uint8), (3, 3, 8)).copy()


def test_listed_labels_go_to_the_background():
    out = _remove(_labels(), labels='2, 5')
    assert out.dtype == np.uint8
    assert sorted(np.unique(out)) == [0, 1, 3, 4, 6, 7]
    assert np.all(out[:, :, 2] == 0) and np.all(out[:, :, 5] == 0)
    assert np.all(out[:, :, 3] == 3)


def test_ranges_and_loose_separators_are_accepted():
    out = _remove(_labels(), labels='1-3; 6 7')
    assert sorted(np.unique(out)) == [0, 4, 5]
    out = _remove(_labels(), labels='3 - 1')
    assert sorted(np.unique(out)) == [0, 4, 5, 6, 7]


def test_an_empty_list_changes_nothing():
    arr = _labels()
    assert np.array_equal(_remove(arr, labels=''), arr)
    assert np.array_equal(_remove(arr, labels=' , '), arr)


def test_unreadable_entries_are_refused():
    with pytest.raises(Exception):
        _remove(_labels(), labels='2, two')


def test_renumbering_closes_the_gaps_in_order():
    out = _remove(_labels(), labels='1, 4', renumber=True)
    # 2, 3, 5, 6, 7 become 1..5
    assert sorted(np.unique(out)) == [0, 1, 2, 3, 4, 5]
    assert np.all(out[:, :, 2] == 1) and np.all(out[:, :, 3] == 2)
    assert np.all(out[:, :, 5] == 3) and np.all(out[:, :, 7] == 5)


def test_renumbering_narrows_the_label_type():
    arr = _labels().astype(np.uint16) * 1000
    out = _remove(arr, labels='7000', renumber=True)
    assert out.dtype == np.uint8
    assert out.max() == 6


def test_float_data_is_refused():
    with pytest.raises(Exception):
        _remove(_labels().astype(np.float32), labels='1')

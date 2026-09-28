"""The batch-processing example in examples/batch really runs: one
tomviz-pipeline call over every generated input, one label map each."""
import importlib.util
from pathlib import Path

import pytest
from click.testing import CliRunner

from tomviz_pipeline.cli import main
from tomviz_pipeline.io import load_dataset
from tomviz_pipeline.writers import writer_for

EXAMPLE = Path(__file__).parents[2] / 'examples' / 'batch' / 'make_example.py'


def _make_example():
    spec = importlib.util.spec_from_file_location('make_example', EXAMPLE)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_batch_example_labels_every_input(tmp_path):
    # The example's leaf is a label map, which tomviz-pipeline only
    # writes from 3.1.7 on; an older package writes nothing for it.
    if writer_for('LabelMap') is None:
        pytest.skip('the installed tomviz-pipeline has no LabelMap writer '
                    '(needs tomviz-pipeline >= 3.1.7)')
    make_example = _make_example()
    state_path, inputs, spheres = make_example.build(tmp_path, count=3,
                                                     size=24)
    assert len(inputs) == 3

    result = CliRunner().invoke(main, [
        '-s', str(state_path),
        '-o', str(tmp_path / 'results'),
        '--input', str(tmp_path / 'inputs' / '*.emd'),
        '-p', 'tqdm',
    ])
    assert result.exit_code == 0, result.output

    for index, expected in enumerate(spheres):
        written = list((tmp_path / 'results' / f'run_{index}').glob('*.emd'))
        assert len(written) == 1, written
        assert written[0].name == '4_Connected_Components__volume.emd'
        labels = load_dataset(written[0]).active_scalars
        # Connected Components numbers the spheres 1..N, so the largest
        # label is the number of spheres that were placed.
        assert int(labels.max()) == expected

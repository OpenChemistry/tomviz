import json
import sys

import numpy as np
import pytest


def test_transform_method_wrapper_internal():
    """Verify transform_method_wrapper calls internal transform when no
    external execution mode is specified."""
    from tomviz._internal import transform_method_wrapper

    called = {'count': 0}

    def dummy_transform(*args, **kwargs):
        called['count'] += 1
        return True

    # Set apply_to_each_array to false so no dataset-expecting wrapper
    # is added around our dummy function
    operator_dict = {
        'label': 'Test',
        'script': '',
        'description': json.dumps({
            'name': 'Test',
            'apply_to_each_array': False,
        }),
    }
    operator_serialized = json.dumps(operator_dict)

    result = transform_method_wrapper(dummy_transform, operator_serialized)
    assert called['count'] == 1
    assert result is True


def test_transform_single_external_operator():
    """Verify that an operator can be executed in a subprocess via
    transform_single_external_operator."""
    from pathlib import Path
    from tomviz._internal import transform_single_external_operator
    from tomviz.external_dataset import Dataset

    # Find the environment that has tomviz-pipeline installed.
    # Use sys.prefix for the current environment, but also check
    # the conda env path since tests may run from a different prefix.
    tomviz_pipeline_env = sys.prefix
    exec_path = Path(tomviz_pipeline_env) / 'bin' / 'tomviz-pipeline'
    if not exec_path.exists():
        # Try the conda env path
        conda_prefix = sys.environ.get('CONDA_PREFIX')
        if conda_prefix:
            tomviz_pipeline_env = conda_prefix
            exec_path = Path(tomviz_pipeline_env) / 'bin' / 'tomviz-pipeline'

    if not exec_path.exists():
        pytest.skip('tomviz-pipeline not found')

    # Create a simple 4x4x4 dataset with all values = 5.0
    data = np.full((4, 4, 4), 5.0, dtype=np.float64)
    dataset = Dataset({'scalars': data}, active='scalars')

    # A simple operator script that adds 10 to all scalars
    script = (
        "def transform(dataset):\n"
        "    dataset.active_scalars = dataset.active_scalars + 10.0\n"
    )

    operator_dict = {
        'type': 'python',
        'label': 'AddTen',
        'script': script,
        'description': json.dumps({
            'name': 'AddTen',
            'apply_to_each_array': False,
            'tomviz_pipeline_env': tomviz_pipeline_env,
        }),
    }
    operator_serialized = json.dumps(operator_dict)

    # The transform_method is not actually called in the external path
    # (the subprocess loads the script from the operator dict), but
    # we still need to pass one.
    def dummy_transform(dataset):
        pass

    transform_single_external_operator(
        dummy_transform, operator_serialized, dataset)

    # The function modifies the input dataset in-place with the subprocess
    # results. Verify that the scalars were updated from 5.0 to 15.0.
    result = dataset.active_scalars
    np.testing.assert_allclose(result, 15.0)

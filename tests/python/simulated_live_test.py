"""The Simulated Live Acquisition sample source delivers projections as
wall-clock time passes and asks for a re-run only when new ones exist."""
import importlib.util
import json

import numpy as np

from utils import OPERATOR_PATH

NAME = 'SimulatedLiveAcquisition'


def _load():
    spec = importlib.util.spec_from_file_location(NAME,
                                                  OPERATOR_PATH / f'{NAME}.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    description = json.loads((OPERATOR_PATH / f'{NAME}.json').read_text())
    return module, description


def _defaults(description, **overrides):
    params = {p['name']: p['default'] for p in description['parameters']}
    params.update(overrides)
    return params


def _kernel():
    from tomviz_pipeline._internal import OperatorWrapper

    module, description = _load()
    kernel = getattr(module, NAME)()
    # What the runtime injects before calling the user's methods.
    kernel._operator_wrapper = OperatorWrapper()
    kernel._parameter_spec = {p['name']: p
                              for p in description['parameters']}
    # A long period, so the tests move the clock instead of sleeping.
    params = _defaults(description, size=16, num_projections=4,
                       seconds_per_projection=10.0)
    return kernel, params


def _run(kernel, params):
    """produce(), then install its write-backs the way the host does."""
    kernel._parameter_updates = {}
    series = kernel.produce(**params)['tilt_series']
    params.update(kernel._parameter_updates)
    return series


def _age(kernel, seconds):
    kernel.state['started_at'] -= seconds


def test_first_run_starts_the_scan_with_one_projection():
    kernel, params = _kernel()

    # Nothing is running yet: the first run of produce starts the clock.
    assert kernel.should_auto_execute(**params) is False

    series = _run(kernel, params)
    assert series.active_scalars.shape[2] == 1
    assert series.tilt_angles.shape == (1,)
    assert series.tilt_angles[0] == params['start_angle']
    assert params['acquired'] == 1

    # The projection is delivered already, so no re-run is due.
    assert kernel.should_auto_execute(**params) is False


def test_new_projections_request_a_rerun_and_arrive_in_order():
    kernel, params = _kernel()
    _run(kernel, params)

    # Two periods later two more projections have been recorded.
    _age(kernel, 25.0)
    assert kernel.should_auto_execute(**params) is True

    series = _run(kernel, params)
    assert series.active_scalars.shape[2] == 3
    assert params['acquired'] == 3
    expected = np.linspace(params['start_angle'], params['end_angle'],
                           params['num_projections'])[:3]
    assert np.allclose(series.tilt_angles, expected)
    # Every projection is of the same object, so their sums agree.
    sums = series.active_scalars.sum(axis=(0, 1))
    assert np.allclose(sums, sums[0], rtol=0.05)


def test_a_loaded_scan_starts_again():
    """A state file keeps the acquired count but not self.state, so a
    scan that was under way when it was saved starts over."""
    kernel, params = _kernel()
    params['acquired'] = 3
    assert kernel.should_auto_execute(**params) is True
    series = _run(kernel, params)
    assert series.active_scalars.shape[2] == 1
    assert params['acquired'] == 1


def test_a_finished_scan_stops_asking():
    kernel, params = _kernel()
    _run(kernel, params)

    _age(kernel, 1000.0)
    assert kernel.should_auto_execute(**params) is True
    series = _run(kernel, params)
    assert series.active_scalars.shape[2] == params['num_projections']

    _age(kernel, 1000.0)
    assert kernel.should_auto_execute(**params) is False


def test_real_node_round_trip():
    """The hook and the write-backs work through the tomviz_pipeline
    node, the way the application and the CLI drive them."""
    from tomviz_pipeline.nodes.sources.python_source import PythonSource

    _, description = _load()
    node = PythonSource()
    assert node.deserialize({
        'description': (OPERATOR_PATH / f'{NAME}.json').read_text(),
        'script': (OPERATOR_PATH / f'{NAME}.py').read_text(),
        'arguments': _defaults(description, size=16, num_projections=3,
                               seconds_per_projection=10.0),
    })
    # A tilt series, so the reconstruction operators accept it
    assert node.output_port('tilt_series').port_type == 'TiltSeries'
    assert node.query_should_auto_execute() is False
    assert node.execute() is True
    assert node.parameters['acquired'] == 1
    assert node.query_should_auto_execute() is False

    node.user_state['started_at'] -= 100.0
    assert node.query_should_auto_execute() is True
    assert node.execute() is True
    assert node.parameters['acquired'] == 3
    data = node.output_port('tilt_series').data().payload
    assert data.active_scalars.shape[2] == 3
    assert node.query_should_auto_execute() is False

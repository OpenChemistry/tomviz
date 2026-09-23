"""The ptycho and pyxrf sources absorb newly arrived scans into their
own parameters (self.set_parameter) during the auto-execute check."""
import importlib.util
import json
from pathlib import Path

import numpy as np

from utils import OPERATOR_PATH

SIMULATION_PATH = Path(__file__).parents[1] / 'simulation'


def _load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _load_source(name):
    module = _load_module(OPERATOR_PATH / f'{name}.py', name)
    spec = json.loads((OPERATOR_PATH / f'{name}.json').read_text())
    return module, {p['name']: p for p in spec['parameters']}


def _make_kernel(cls, spec):
    # The host injects the declared spec before calling the hook, which
    # turns set_parameter validation on.
    kernel = cls()
    kernel._parameter_spec = spec
    return kernel


def _ptycho_sim():
    return _load_module(SIMULATION_PATH / 'simulate_ptycho_stream.py',
                        'ptycho_sim')


def _ptycho_params(root, sids, versions, angles, use=None):
    use = use if use is not None else [True] * len(sids)
    ui_state = {
        'filter_sids_string': '',
        'csv_file': '',
        'full_sid_list': list(sids),
        'full_version_list': list(versions),
        'full_use_list': list(use),
    }
    used = [i for i, u in enumerate(use) if u]
    return {
        'ptycho_dir': str(root),
        'output_info_file': '',
        'rotate_datasets': True,
        'sid_list': json.dumps([sids[i] for i in used]),
        'version_list': json.dumps([versions[i] for i in used]),
        'angle_list': json.dumps([angles[i] for i in used]),
        'ui_state': json.dumps(ui_state),
    }


def test_ptycho_absorbs_new_scans(tmp_path):
    sim = _ptycho_sim()
    mod, spec = _load_source('PtychoSource')
    rng = np.random.default_rng(0)

    sim.write_scan(tmp_path, 50001, -60.0, sim.BASE_SHAPE, rng)
    sim.write_scan(tmp_path, 50002, -58.0, sim.BASE_SHAPE, rng)

    kernel = _make_kernel(mod.PtychoSource, spec)
    # Scan 50002 was deselected by the user; that choice must survive.
    params = _ptycho_params(tmp_path, [50001, 50002], ['t1', 't1'],
                            [-60.0, -58.0], use=[True, False])

    # First check records the baseline
    assert kernel.should_auto_execute(**params) is False
    assert kernel._parameter_updates == {}

    sim.write_scan(tmp_path, 50003, -56.0, sim.BASE_SHAPE, rng)
    assert kernel.should_auto_execute(**params) is True

    updates = kernel._parameter_updates
    assert json.loads(updates['sid_list']) == [50001, 50003]
    assert json.loads(updates['version_list']) == ['t1', 't1']
    assert abs(json.loads(updates['angle_list'])[1] + 56.0) < 1e-6

    ui_state = json.loads(updates['ui_state'])
    assert ui_state['full_sid_list'] == [50001, 50002, 50003]
    # 50002 stays deselected; the new scan arrives selected
    assert ui_state['full_use_list'] == [True, False, True]

    # A quiet directory answers no and writes nothing
    kernel._parameter_updates = {}
    assert kernel.should_auto_execute(**params) is False
    assert kernel._parameter_updates == {}


def test_ptycho_ignores_incomplete_scans(tmp_path):
    sim = _ptycho_sim()
    mod, spec = _load_source('PtychoSource')
    rng = np.random.default_rng(0)

    sim.write_scan(tmp_path, 50001, -60.0, sim.BASE_SHAPE, rng)
    kernel = _make_kernel(mod.PtychoSource, spec)
    params = _ptycho_params(tmp_path, [50001], ['t1'], [-60.0])
    assert kernel.should_auto_execute(**params) is False

    # Object written but no probe yet: re-run, but don't absorb
    recon = tmp_path / 'S50002' / 't1' / 'recon_data'
    recon.mkdir(parents=True)
    (recon / '50002_t1.txt').write_text('angle = -58.0\n')
    np.save(recon / 'recon_50002_t1_object_ave.npy',
            np.zeros((4, 4), np.complex64))
    assert kernel.should_auto_execute(**params) is True
    assert 'sid_list' not in kernel._parameter_updates

    # The probe arrives: now it is absorbed
    np.save(recon / 'recon_50002_t1_probe_ave.npy',
            np.zeros((4, 4), np.complex64))
    assert kernel.should_auto_execute(**params) is True
    assert json.loads(kernel._parameter_updates['sid_list']) == \
        [50001, 50002]


def test_ptycho_absorbs_a_scan_whose_config_lands_last(tmp_path):
    sim = _ptycho_sim()
    mod, spec = _load_source('PtychoSource')
    rng = np.random.default_rng(0)

    sim.write_scan(tmp_path, 50001, -60.0, sim.BASE_SHAPE, rng)
    kernel = _make_kernel(mod.PtychoSource, spec)
    params = _ptycho_params(tmp_path, [50001], ['t1'], [-60.0])
    assert kernel.should_auto_execute(**params) is False

    # Both arrays first: a re-run, but nothing to absorb without an angle
    recon = tmp_path / 'S50002' / 't1' / 'recon_data'
    recon.mkdir(parents=True)
    np.save(recon / 'recon_50002_t1_object_ave.npy',
            np.zeros((4, 4), np.complex64))
    np.save(recon / 'recon_50002_t1_probe_ave.npy',
            np.zeros((4, 4), np.complex64))
    assert kernel.should_auto_execute(**params) is True
    assert 'sid_list' not in kernel._parameter_updates

    # The config alone arriving completes the scan and must be noticed
    (recon / '50002_t1.txt').write_text('angle = -58.0\n')
    assert kernel.should_auto_execute(**params) is True
    assert json.loads(kernel._parameter_updates['sid_list']) == \
        [50001, 50002]


def test_ptycho_produce_includes_absorbed_scans(tmp_path):
    sim = _ptycho_sim()
    mod, spec = _load_source('PtychoSource')
    rng = np.random.default_rng(0)

    sim.write_scan(tmp_path, 50001, -60.0, sim.BASE_SHAPE, rng)
    sim.write_scan(tmp_path, 50002, -58.0, sim.BASE_SHAPE, rng)
    kernel = _make_kernel(mod.PtychoSource, spec)
    params = _ptycho_params(tmp_path, [50001, 50002], ['t1', 't1'],
                            [-60.0, -58.0])
    assert kernel.should_auto_execute(**params) is False

    sim.write_scan(tmp_path, 50003, -56.0, sim.BASE_SHAPE, rng)
    assert kernel.should_auto_execute(**params) is True

    # The host installs the write-backs before the run
    params.update(kernel._parameter_updates)
    outputs = kernel.produce(**params)
    assert sorted(outputs['object'].scan_ids.tolist()) == \
        [50001, 50002, 50003]


def test_ptycho_write_back_through_real_node(tmp_path):
    """The full harvest chain with the real tomviz_pipeline node."""
    from tomviz_pipeline.nodes.sources.python_source import PythonSource

    sim = _ptycho_sim()
    rng = np.random.default_rng(0)
    sim.write_scan(tmp_path, 50001, -60.0, sim.BASE_SHAPE, rng)

    node = PythonSource()
    assert node.deserialize({
        'description': (OPERATOR_PATH / 'PtychoSource.json').read_text(),
        'script': (OPERATOR_PATH / 'PtychoSource.py').read_text(),
        'arguments': _ptycho_params(tmp_path, [50001], ['t1'], [-60.0]),
    })
    assert node.query_should_auto_execute() is False

    sim.write_scan(tmp_path, 50002, -58.0, sim.BASE_SHAPE, rng)
    assert node.query_should_auto_execute() is True
    assert json.loads(node.parameters['sid_list']) == [50001, 50002]
    assert json.loads(node.parameters['ui_state'])['full_sid_list'] == \
        [50001, 50002]


def _touch_scans(root, sids):
    for sid in sids:
        (root / f'scan2D_{sid}.h5').write_bytes(b'')


def test_grown_scan_range_grammar(tmp_path):
    mod, _ = _load_source('PyXRFSource')
    grow = mod._grown_scan_range

    _touch_scans(tmp_path, [100, 101, 102, 103, 105])
    assert grow('100:102', tmp_path) == '100:105'
    assert grow('100:102:2', tmp_path) == '100:105:2'
    assert grow('100', tmp_path) == '100, 101:105'
    # Nothing beyond the range, or no usable range: no growth
    assert grow('100:200', tmp_path) is None
    assert grow('', tmp_path) is None
    assert grow('junk', tmp_path) is None


def test_pyxrf_grows_range_on_new_scans(tmp_path):
    mod, spec = _load_source('PyXRFSource')
    _touch_scans(tmp_path, [100, 101])

    kernel = _make_kernel(mod.PyXRFSource, spec)
    params = {'working_directory': str(tmp_path), 'scan_range': '100:101'}
    assert kernel.should_auto_execute(**params) is False

    _touch_scans(tmp_path, [102])
    assert kernel.should_auto_execute(**params) is True
    assert kernel._parameter_updates == {'scan_range': '100:102'}

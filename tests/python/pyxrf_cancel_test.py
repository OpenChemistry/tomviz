"""Canceling the PyXRF source stops pyxrf-utils, not just its wrapper."""
import importlib.util
import sys
import time
from pathlib import Path

import pytest

from utils import OPERATOR_PATH

_spec = importlib.util.spec_from_file_location(
    'PyXRFSource', OPERATOR_PATH / 'PyXRFSource.py')
PyXRFSource = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(PyXRFSource)


@pytest.mark.skipif(sys.platform == 'win32',
                    reason='pyxrf-utils runs on Linux only')
def test_cancel_stops_the_whole_command(tmp_path: Path) -> None:
    # Like run-pyxrf-utils: a shell script whose python child does the
    # work. The child holds the output open, so the command only returns
    # once the child itself has stopped.
    started = tmp_path / 'started'
    child = tmp_path / 'pyxrf_utils.py'
    child.write_text('import pathlib, time\n'
                     f'pathlib.Path(r"{started}").touch()\n'
                     'time.sleep(60)\n')
    script = tmp_path / 'run-pyxrf-utils'
    script.write_text(f'#!/bin/sh\n"{sys.executable}" "{child}"\n')
    script.chmod(0o755)

    start = time.monotonic()
    PyXRFSource._run_command([str(script)], canceled=started.exists)
    assert time.monotonic() - start < 5

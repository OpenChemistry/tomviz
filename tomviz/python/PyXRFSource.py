from __future__ import annotations

import json
import os
import queue
import re
import signal
import subprocess
import tempfile
import threading
import time
from collections.abc import Callable
from pathlib import Path
from typing import TYPE_CHECKING

import h5py
import numpy as np
from numpy.typing import NDArray

import tomviz.nodes

if TYPE_CHECKING:
    from tomviz.dataset import Dataset


def _rotate_stack_minus_90(array: NDArray) -> NDArray:
    # Exact quarter turn: identical result to
    # scipy.ndimage.rotate(array, -90.0, axes=(1, 2)) but with no
    # interpolation and orders of magnitude faster.
    return np.rot90(array, k=-1, axes=(1, 2))


def _dir_snapshot(working_directory: str | Path) -> dict[str, int]:
    # The scan files and tomo.h5, with their modification times
    root = Path(working_directory)
    snapshot = {}
    for path in sorted(root.glob('scan2D_*.h5')) + [root / 'tomo.h5']:
        try:
            snapshot[path.name] = path.stat().st_mtime_ns
        except OSError:
            continue
    return snapshot


def _is_fitted(path: Path) -> bool:
    try:
        with h5py.File(path, 'r') as f:
            return 'xrfmap/detsum/xrf_fit' in f
    except OSError:
        # Missing, or still open for writing elsewhere
        return False


def _compact_range(scan_ids: list[int]) -> str:
    """Turn sorted scan IDs into a range string, such as '100:103, 105'"""
    runs: list[list[int]] = []
    for sid in scan_ids:
        if runs and sid == runs[-1][1] + 1:
            runs[-1][1] = sid
        else:
            runs.append([sid, sid])
    return ', '.join(f'{a}:{b}' if a != b else f'{a}' for a, b in runs)


def _expand_scan_range(scan_range: str,
                       skip_ids: list[int] | None = None) -> list[int]:
    if not scan_range or not scan_range.strip():
        return []
    skip_set = set(skip_ids) if skip_ids else set()
    ids: set[int] = set()
    for part in scan_range.split(','):
        part = part.strip()
        if not part:
            continue
        if ':' in part:
            pieces = part.split(':')
            start = int(pieces[0])
            stop = int(pieces[1])
            stride = int(pieces[2]) if len(pieces) > 2 else 1
            ids.update(range(start, stop + 1, stride))
        else:
            ids.add(int(part))
    return sorted(ids - skip_set)


_SCAN_FILE_RE = re.compile(r'^scan2D_(\d+)\.h5$')


def _scan_ids_on_disk(working_directory: str | Path) -> list[int]:
    ids = []
    for path in Path(working_directory).glob('scan2D_*.h5'):
        match = _SCAN_FILE_RE.match(path.name)
        if match:
            ids.append(int(match.group(1)))
    return sorted(ids)


def _grown_scan_range(scan_range: str,
                      working_directory: str | Path) -> str | None:
    """Extend the range to cover scans that appeared past its end.

    Returns the grown range string, or None when there is nothing to
    grow. Scans below the range's start or inside its holes are left
    alone: the range is user intent, and what a live acquisition adds
    is new scans past the end.
    """
    try:
        covered = _expand_scan_range(scan_range)
    except (ValueError, IndexError):
        return None
    if not covered:
        return None
    top = covered[-1]

    beyond = [i for i in _scan_ids_on_disk(working_directory) if i > top]
    if not beyond:
        return None
    new_stop = beyond[-1]

    segments = [s.strip() for s in scan_range.split(',') if s.strip()]
    pieces = segments[-1].split(':')
    try:
        # If the last segment ends the range, just bump its stop and keep
        # any stride. Compare the last scan it covers rather than its
        # stop, since a stop grown earlier may be off the stride.
        # Otherwise, append a new segment.
        if (len(pieces) in (2, 3) and
                _expand_scan_range(segments[-1])[-1] == top):
            pieces[1] = str(new_stop)
            segments[-1] = ':'.join(pieces)
        else:
            raise ValueError
    except (ValueError, IndexError):
        segments.append(f'{beyond[0]}:{new_stop}'
                        if beyond[0] != new_stop else f'{new_stop}')
    grown = ', '.join(segments)

    # Only count it as growth if it takes in a scan on disk. Otherwise, a
    # new scan off the stride would re-run the source on every check.
    if not set(beyond) & set(_expand_scan_range(grown)):
        return None
    return grown


def _signal_group(proc: subprocess.Popen, sig: int) -> None:
    try:
        os.killpg(proc.pid, sig)
    except ProcessLookupError:
        pass  # Everything in it has already exited


def _run_command(args: list[str], canceled: Callable[[], bool]) -> None:
    env = os.environ.copy()
    env['PYTHONUNBUFFERED'] = '1'
    print(f'Running: {" ".join(str(a) for a in args)}', flush=True)
    # A session of its own lets cancel signal the whole process group:
    # run-pyxrf-utils is a bash script running pixi running python, and
    # stopping only bash would leave the download or fit going.
    proc = subprocess.Popen(
        args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, errors='replace', bufsize=1, env=env,
        start_new_session=True,
    )
    stdout = proc.stdout
    assert stdout is not None

    # Read on a thread so cancel is noticed while the command is quiet
    # (a single scan can download for minutes); print here.
    lines: queue.Queue[str | None] = queue.Queue()

    def read() -> None:
        for line in stdout:
            lines.put(line)
        lines.put(None)

    threading.Thread(target=read, daemon=True).start()

    # Interrupt first so pyxrf-utils can clean up (and h5py close its
    # files), then kill whatever is still running 10 s later.
    kill_at = None
    output_open = True
    while output_open or proc.poll() is None:
        if kill_at is None and canceled():
            print('Canceling pyxrf-utils...', flush=True)
            _signal_group(proc, signal.SIGINT)
            kill_at = time.monotonic() + 10
        elif kill_at is not None and time.monotonic() > kill_at:
            _signal_group(proc, signal.SIGKILL)
        try:
            line = lines.get(timeout=0.1)
        except queue.Empty:
            continue
        if line is None:
            output_open = False
        else:
            print(line, end='', flush=True)

    if kill_at is not None:
        return  # Canceled; produce() checks before going on
    if proc.returncode != 0:
        raise RuntimeError(
            f'Command failed with exit code {proc.returncode}: '
            f'{" ".join(str(a) for a in args)}'
        )


def _run_make_hdf5(command: str, working_directory: Path,
                   scan_range: str, skip_scan_ids: list[int],
                   force: bool, canceled: Callable[[], bool]) -> None:
    args = [
        command, 'make-hdf5', str(working_directory),
        '--range', scan_range,
    ]
    if skip_scan_ids:
        args += ['--skip', json.dumps(skip_scan_ids)]
    if force:
        args.append('--force')
    _run_command(args, canceled)


def _run_process_projections(command: str, working_directory: Path,
                             scan_range: str, skip_scan_ids: list[int],
                             parameters_file: str, ic_name: str,
                             output_directory: Path,
                             skip_processed: bool,
                             csv_output: str,
                             canceled: Callable[[], bool]) -> None:
    args = [
        command, 'process-projections', str(working_directory),
        '--range', scan_range,
        '-p', str(parameters_file),
        '-i', ic_name,
        '-o', str(output_directory),
    ]
    if skip_scan_ids:
        args += ['--skip', json.dumps(skip_scan_ids)]
    if skip_processed:
        args.append('-s')
    if csv_output:
        args += ['--csv-output', csv_output]
    _run_command(args, canceled)


def _read_hdf5_metadata(
    working_directory: Path,
    scan_ids: list[int],
) -> tuple[tuple[float, float] | None, list[int], NDArray[np.floating] | None]:
    """Read pixel sizes, valid scan IDs, and theta from HDF5 files."""
    pixel_sizes: tuple[float, float] | None = None
    valid_ids: list[int] = []
    thetas: list[float] = []

    for sid in scan_ids:
        path = working_directory / f'scan2D_{sid}.h5'
        if not path.exists():
            continue
        try:
            with h5py.File(path, 'r') as f:
                md = f['xrfmap/scan_metadata']
                theta = float(md.attrs['param_theta'])
                theta_units = md.attrs.get('param_theta_units', 'deg')
                if theta_units == 'mdeg':
                    theta /= 1000.0
                valid_ids.append(sid)
                thetas.append(theta)

                if pixel_sizes is None:
                    param_input = md.attrs['param_input']
                    x_start, x_stop, num_x = param_input[0], param_input[1], param_input[2]
                    y_start, y_stop, num_y = param_input[3], param_input[4], param_input[5]
                    if num_x > 0 and num_y > 0:
                        px = round((x_stop - x_start) / num_x * 1e3, 8)
                        py = round((y_stop - y_start) / num_y * 1e3, 8)
                        pixel_sizes = (px, py)
                        print(f'Pixel sizes from HDF5: {px} {py}')
        except Exception as exc:
            print(f'Warning: failed to read metadata from {path.name}: {exc}')

    # tomo.h5 sorts the projections by angle, so we need to match that
    # (the scans may have been collected from high angles to low).
    order = sorted(range(len(thetas)), key=lambda i: thetas[i])
    valid_ids = [valid_ids[i] for i in order]
    thetas = [thetas[i] for i in order]

    theta_array = np.array(thetas, dtype=np.float64) if thetas else None
    return pixel_sizes, valid_ids, theta_array


def _read_tomo_h5(tomo_file: Path, rotate_datasets: bool,
                  pixel_sizes: tuple[float, float] | None,
                  scan_ids: list[int],
                  theta: NDArray[np.floating] | None,
                  create_dataset: Callable[[], Dataset]) -> Dataset:
    with h5py.File(tomo_file, 'r') as f:
        element_names: list[str] = [
            x.decode() if isinstance(x, bytes) else x
            for x in f['reconstruction/fitting/elements'][()]
        ]
        data: NDArray[np.floating] = f['reconstruction/fitting/data'][()]
        file_theta: NDArray[np.floating] = f['exchange/theta'][()]

    # The scan files have more precise angles than tomo.h5. But only use
    # them (and the scan IDs) if they match the projections in tomo.h5.
    if theta is None or theta.shape != file_theta.shape or not np.allclose(
            theta, file_theta, atol=2e-3):
        if theta is not None:
            print('Warning: the scan files do not match tomo.h5. Using '
                  'the angles from tomo.h5 instead.')
        theta = file_theta
        scan_ids = []

    ds = create_dataset()

    for i, name in enumerate(element_names):
        element_data = data[:, i, :, :]
        if rotate_datasets:
            element_data = _rotate_stack_minus_90(element_data)
        element_data = element_data.swapaxes(0, 2)
        ds.set_scalars(name, element_data)

    ds.tilt_angles = theta
    ds.tilt_axis = 2

    if pixel_sizes is not None:
        ds.spacing = (pixel_sizes[0], pixel_sizes[1], 1)

    if scan_ids:
        ds.scan_ids = np.array(scan_ids, dtype=np.int32)

    return ds


class PyXRFSource(tomviz.nodes.SourceNode):

    def should_auto_execute(self, **parameters) -> bool:
        # Watch the working directory: a newly downloaded scan2D file or
        # a refreshed tomo.h5 changes the snapshot and requests a re-run.
        # The first check only records the current state, so enabling
        # periodic execution does not immediately reprocess data that is
        # already in.
        working_directory = parameters.get('working_directory', '')
        if not working_directory or not Path(working_directory).is_dir():
            return False

        snapshot = _dir_snapshot(working_directory)
        previous = self.state.get('dir_snapshot')
        self.state['dir_snapshot'] = snapshot
        changed = previous is not None and snapshot != previous

        # Grow the range if scans showed up past its end, so the re-run
        # includes them. We check this every time, rather than only on a
        # change, in case scans arrived before the first check. With no
        # range, produce() processes every scan that is present.
        scan_range = parameters.get('scan_range', '')
        if scan_range:
            grown = _grown_scan_range(scan_range, working_directory)
            if grown is not None:
                print(f'New scans detected; growing range to {grown}')
                self.set_parameter('scan_range', grown)
                return True

        return changed

    def produce(self, pyxrf_utils_command: str = 'pyxrf-utils',
                working_directory: str = '',
                scan_range: str = '',
                skip_scan_ids: str = '[]',
                skip_downloads: bool = False,
                redownload_successful: bool = False,
                parameters_file: str = '',
                ic_name: str = 'sclr1_ch4',
                skip_processed: bool = True,
                rotate_datasets: bool = True,
                csv_output: str = '',
                ui_state: str = '{}') -> dict[str, Dataset] | None:
        working_dir = Path(working_directory)
        skip_ids: list[int] = json.loads(skip_scan_ids) if skip_scan_ids else []

        if scan_range and not skip_downloads:
            _run_make_hdf5(
                pyxrf_utils_command, working_dir,
                scan_range, skip_ids, redownload_successful,
                lambda: self.canceled,
            )
            if self.canceled:
                return None

        on_disk = _scan_ids_on_disk(working_dir)
        if scan_range:
            range_arg = scan_range
            scan_ids = _expand_scan_range(scan_range, skip_ids)
        elif parameters_file and on_disk:
            # No range: process every scan in the directory
            scan_ids = [sid for sid in on_disk if sid not in skip_ids]
            range_arg = _compact_range(scan_ids)
            print(f'No scan range set. Processing the scans on disk: '
                  f'{range_arg}', flush=True)
        else:
            # Nothing to process. Just load a tomo.h5 made elsewhere.
            if not (working_dir / 'tomo.h5').exists():
                raise RuntimeError(
                    f'No scans to process and no tomo.h5 in {working_dir}. '
                    'Set a scan range to download scans, or choose a '
                    'parameters file to process the scan2D_<id>.h5 files '
                    'in the directory.'
                )
            return self._read_results(working_dir, working_dir, [],
                                      rotate_datasets)

        if not scan_ids:
            raise RuntimeError('All of the scans are skipped')

        try:
            with tempfile.TemporaryDirectory() as tmp_dir:
                output_dir = Path(tmp_dir)
                _run_process_projections(
                    pyxrf_utils_command, working_dir,
                    range_arg, skip_ids,
                    parameters_file, ic_name,
                    output_dir, skip_processed, csv_output,
                    lambda: self.canceled,
                )
                if self.canceled:
                    return None
                return self._read_results(output_dir, working_dir,
                                          scan_ids, rotate_datasets)
        finally:
            self._update_snapshot(working_dir, scan_ids)

    def _update_snapshot(self, working_dir: Path,
                         scan_ids: list[int]) -> None:
        # Fitting writes into the scan files, which changes their
        # modification times. Update the snapshot for those, so the next
        # check does not mistake our own writes for new data and re-run
        # forever. Unfitted scans are left alone, since another process
        # may still be writing them.
        snapshot = self.state.get('dir_snapshot')
        if snapshot is None:
            return  # Periodic execution has not checked yet

        snapshot = dict(snapshot)
        on_disk = set(_scan_ids_on_disk(working_dir))
        for sid in sorted(on_disk.intersection(scan_ids)):
            path = working_dir / f'scan2D_{sid}.h5'
            if not _is_fitted(path):
                continue
            try:
                snapshot[path.name] = path.stat().st_mtime_ns
            except OSError:
                continue
        self.state['dir_snapshot'] = snapshot

    def _read_results(
        self, output_dir: Path, working_dir: Path,
        scan_ids: list[int], rotate_datasets: bool,
    ) -> dict[str, Dataset]:
        tomo_file = output_dir / 'tomo.h5'
        if not tomo_file.exists():
            raise RuntimeError(
                f'tomo.h5 not found at {tomo_file} after processing'
            )

        pixel_sizes, valid_ids, theta = _read_hdf5_metadata(working_dir,
                                                            scan_ids)

        return {
            'elements': _read_tomo_h5(
                tomo_file, rotate_datasets, pixel_sizes,
                valid_ids, theta, self.create_dataset,
            ),
        }

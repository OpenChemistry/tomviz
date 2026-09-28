from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import tempfile
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


def _dir_fingerprint(working_directory: str | Path) -> str:
    # A digest of the scan files (and the assembled tomo.h5, for the
    # no-scan-range workflow) with their mtimes, so a newly downloaded
    # scan or a refreshed assembly changes the fingerprint.
    root = Path(working_directory)
    entries = []
    for path in sorted(root.glob('scan2D_*.h5')) + [root / 'tomo.h5']:
        try:
            entries.append(f'{path.name}:{path.stat().st_mtime}')
        except OSError:
            continue
    return hashlib.sha1('\n'.join(entries).encode()).hexdigest()


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
        # When the last segment's stop is the range's end (the common
        # "start:stop" case, and any segment grown here before), bump
        # it in place so repeated growth stays one compact segment; a
        # stride is preserved. Otherwise append a segment.
        if len(pieces) in (2, 3) and int(pieces[1]) == top:
            pieces[1] = str(new_stop)
            segments[-1] = ':'.join(pieces)
        else:
            raise ValueError
    except (ValueError, IndexError):
        segments.append(f'{beyond[0]}:{new_stop}'
                        if beyond[0] != new_stop else f'{new_stop}')
    return ', '.join(segments)


def _run_command(args: list[str]) -> None:
    env = os.environ.copy()
    env['PYTHONUNBUFFERED'] = '1'
    print(f'Running: {" ".join(str(a) for a in args)}', flush=True)
    proc = subprocess.Popen(
        args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1, env=env,
    )
    assert proc.stdout is not None
    for line in proc.stdout:
        print(line, end='', flush=True)
    proc.wait()
    if proc.returncode != 0:
        raise RuntimeError(
            f'Command failed with exit code {proc.returncode}: '
            f'{" ".join(str(a) for a in args)}'
        )


def _run_make_hdf5(command: str, working_directory: Path,
                   scan_range: str, skip_scan_ids: list[int],
                   force: bool) -> None:
    args = [
        command, 'make-hdf5', str(working_directory),
        '--range', scan_range,
    ]
    if skip_scan_ids:
        args += ['--skip', json.dumps(skip_scan_ids)]
    if force:
        args.append('--force')
    _run_command(args)


def _run_process_projections(command: str, working_directory: Path,
                             scan_range: str, skip_scan_ids: list[int],
                             parameters_file: str, ic_name: str,
                             output_directory: Path,
                             skip_processed: bool,
                             csv_output: str) -> None:
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
    _run_command(args)


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
        if theta is None:
            theta = f['exchange/theta'][()]

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
        # a refreshed tomo.h5 changes the fingerprint and requests a
        # re-run. The first check only records the current state, so
        # enabling periodic execution does not immediately reprocess
        # data that is already in.
        working_directory = parameters.get('working_directory', '')
        if not working_directory or not Path(working_directory).is_dir():
            return False

        fingerprint = _dir_fingerprint(working_directory)
        previous = self.state.get('dir_fingerprint')
        self.state['dir_fingerprint'] = fingerprint
        changed = previous is not None and fingerprint != previous

        if changed:
            # A set range pins downloads and processing, so scans past
            # its end must grow it or the re-run would exclude them.
            # With no range, produce reads whatever is present.
            scan_range = parameters.get('scan_range', '')
            if scan_range:
                grown = _grown_scan_range(scan_range, working_directory)
                if grown is not None:
                    print(f'New scans detected; growing range to {grown}')
                    self.set_parameter('scan_range', grown)

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
                ui_state: str = '{}') -> dict[str, Dataset]:
        working_dir = Path(working_directory)
        skip_ids: list[int] = json.loads(skip_scan_ids) if skip_scan_ids else []

        if scan_range and not skip_downloads:
            _run_make_hdf5(
                pyxrf_utils_command, working_dir,
                scan_range, skip_ids, redownload_successful,
            )

        if scan_range:
            with tempfile.TemporaryDirectory() as tmp_dir:
                output_dir = Path(tmp_dir)
                _run_process_projections(
                    pyxrf_utils_command, working_dir,
                    scan_range, skip_ids,
                    parameters_file, ic_name,
                    output_dir, skip_processed, csv_output,
                )
                return self._read_results(
                    output_dir, working_dir, scan_range,
                    skip_ids, rotate_datasets,
                )

        # No scan_range: expect tomo.h5 already in working_directory
        return self._read_results(
            working_dir, working_dir, '',
            skip_ids, rotate_datasets,
        )

    def _read_results(
        self, output_dir: Path, working_dir: Path,
        scan_range: str, skip_ids: list[int],
        rotate_datasets: bool,
    ) -> dict[str, Dataset]:
        tomo_file = output_dir / 'tomo.h5'
        if not tomo_file.exists():
            raise RuntimeError(
                f'tomo.h5 not found at {tomo_file} after processing'
            )

        all_ids = _expand_scan_range(scan_range, skip_ids) if scan_range else []
        pixel_sizes, valid_ids, theta = _read_hdf5_metadata(working_dir, all_ids)

        return {
            'elements': _read_tomo_h5(
                tomo_file, rotate_datasets, pixel_sizes,
                valid_ids, theta, self.create_dataset,
            ),
        }

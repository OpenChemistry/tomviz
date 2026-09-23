from __future__ import annotations

import csv
import hashlib
import json
import math
import sys
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING

import numpy as np
from numpy.typing import NDArray
from scipy.optimize import leastsq
from tqdm import tqdm

import tomviz.nodes

if TYPE_CHECKING:
    from collections.abc import Callable

    from tomviz.dataset import Dataset


# The ptycho file-layout helpers below are copied from
# tomviz/ptycho/ptycho.py: this script is embedded in state files and
# may run in an external environment without the tomviz.ptycho module.

# Files with these suffixes cannot be the small text config that
# carries the angle and pixel sizes.
NON_CONFIG_SUFFIXES = {
    '.npy', '.npz', '.h5', '.hdf5', '.nxs', '.tif', '.tiff',
    '.png', '.jpg', '.jpeg', '.mat',
}

# The config is a small text file; never read more than this much of
# any candidate while searching for it.
MAX_CONFIG_READ_BYTES = 1024 * 1024


def find_ptycho_file(sid: int, version: str, type_str: str,
                     ptycho_dir: str | Path) -> Path | None:
    # type_str is `ptycho` or `probe`
    ptycho_dir = Path(ptycho_dir)
    dir_path = ptycho_dir / f'S{sid}/{version}/recon_data'
    base_str = f'recon_{sid}_{version}_{type_str}'
    # Prefer `_ave.npy` if available, then `.npy`
    suffix_to_try = [
        '_ave.npy',
        '.npy',
    ]
    for suffix in suffix_to_try:
        path = dir_path / f'{base_str}{suffix}'
        if path.exists():
            return path

    # If those didn't exist, just try to grab anything that
    # matches `{base_str}*.npy`
    paths = list(dir_path.glob(f'{base_str}*.npy'))
    if paths:
        return paths[0].resolve()

    # Didn't find any matches
    return None


def locate_ptycho_hyan_file(sid: int, version: str,
                            ptycho_dir: str | Path) -> Path | None:
    recon_data_dir = Path(ptycho_dir) / f'S{sid}' / version / 'recon_data'
    matches = list(recon_data_dir.glob(f'{sid}_{version}*'))

    for match in matches:
        if match.suffix.lower() in NON_CONFIG_SUFFIXES:
            continue
        try:
            with open(match.resolve(), 'r', encoding='utf-8',
                      errors='replace') as rf:
                if 'angle =' in rf.read(MAX_CONFIG_READ_BYTES):
                    return match.resolve()
        except Exception:
            # Move on to the next one
            continue

    return None


def fetch_angle_from_ptycho_hyan_file(filepath: str | Path) -> float:
    with open(filepath, 'r', encoding='utf-8') as rf:
        for line in rf:
            line = line.lstrip()
            if line.startswith('angle = '):
                return float(line.split('=')[1].strip())

    # Angle was not found
    return math.nan


def _discover_scans(ptycho_dir: str | Path) -> dict[int, tuple[str, float]]:
    """Complete scans on disk: {sid: (version, angle)}.

    A scan counts when some version has the object and probe files and
    an angle, mirroring the dialog's "first valid version" default.
    Incomplete scans are left out and picked up on a later check once
    their remaining files arrive.
    """
    root = Path(ptycho_dir)
    if not root.is_dir():
        return {}

    scans: dict[int, tuple[str, float]] = {}
    for entry in sorted(root.iterdir()):
        if not (entry.name.startswith('S') and entry.is_dir()):
            continue
        try:
            sid = int(entry.name[1:])
        except ValueError:
            continue

        versions = sorted(x.name for x in entry.iterdir() if x.is_dir())
        for version in versions:
            if find_ptycho_file(sid, version, 'object', root) is None:
                continue
            if find_ptycho_file(sid, version, 'probe', root) is None:
                continue
            config = locate_ptycho_hyan_file(sid, version, root)
            if config is None:
                continue
            angle = fetch_angle_from_ptycho_hyan_file(config)
            if math.isnan(angle):
                continue
            scans[sid] = (version, angle)
            break

    return scans


def fetch_pixel_sizes_from_ptycho_hyan_file(
    filepath: str | Path,
) -> tuple[float, float] | None:
    print(f'Obtaining pixel sizes from config file: {filepath}')
    vars_required = [
        'lambda_nm', 'z_m', 'nx', 'ny', 'ccd_pixel_um'
    ]
    alternatives = {
        'nx': 'x_arr_size',
        'ny': 'y_arr_size',
    }
    vars_requested = vars_required + list(alternatives.values())
    results = {}
    try:
        with open(filepath, 'r', encoding='utf-8') as rf:
            for line in rf:
                if '=' not in line:
                    continue

                lhs = line.split('=')[0].strip()
                if lhs in vars_requested:
                    value = float(line.split('=', 1)[1].strip())
                    results[lhs] = value
    except Exception as e:
        print('Failed to fetch pixel sizes with error:', e, file=sys.stderr)
        return None

    # Add alternatives if they are present
    for name in vars_required:
        if name not in results and name in alternatives:
            # Check the alt_name
            alt_name = alternatives[name]
            if alt_name in results:
                # Convert it
                results[name] = results.pop(alt_name)

    missing = [x for x in vars_required if x not in results]
    if missing:
        print(
            'Failed to fetch pixel sizes. Some required variables '
            f'were not found: {missing}'
        )
        return None

    # Now compute them. They can both use the same numerator
    numerator = (
        results['lambda_nm'] * results['z_m'] * 1e6 / results['ccd_pixel_um']
    )

    x_pixel_size = numerator / results['nx']
    y_pixel_size = numerator / results['ny']

    return x_pixel_size, y_pixel_size


def _rotate_stack_minus_90(array: NDArray) -> NDArray:
    # Exact quarter turn: identical result to
    # scipy.ndimage.rotate(array, -90.0, axes=(1, 2)) but with no
    # interpolation and orders of magnitude faster.
    return np.rot90(array, k=-1, axes=(1, 2))


def _cache_dir_for(ptycho_dir: str | Path) -> Path:
    # Per-scan processing cache, keyed by the data directory so several
    # datasets never collide. Lives in the system temp dir to keep the
    # (possibly read-only) data directory untouched.
    digest = hashlib.sha1(
        str(Path(ptycho_dir).resolve()).encode()).hexdigest()[:16]
    return Path(tempfile.gettempdir()) / 'tomviz-ptycho-cache' / digest


def _dir_fingerprint(ptycho_dir: str | Path) -> str:
    # A digest of the reconstruction files present and their mtimes, so
    # a new scan or an updated reconstruction changes the fingerprint.
    # The small config beside them counts too: a scan is only complete
    # once it has arrived, and it can land after the arrays.
    root = Path(ptycho_dir)
    entries = []
    for path in sorted(root.glob('S*/*/recon_data/*')):
        name = path.name
        is_recon = name.startswith('recon_') and name.endswith('.npy')
        is_config = (name[:1].isdigit() and
                     path.suffix.lower() not in NON_CONFIG_SUFFIXES)
        if not (is_recon or is_config):
            continue
        try:
            entries.append(f'{path.relative_to(root)}:{path.stat().st_mtime}')
        except OSError:
            continue
    return hashlib.sha1('\n'.join(entries).encode()).hexdigest()


def _load_cached_scan(cache_path: Path,
                      src_mtimes: list[float]) -> tuple | None:
    if not cache_path.exists():
        return None
    try:
        with np.load(cache_path) as loaded:
            # Exact comparison: at epoch magnitudes, allclose-style
            # tolerances would accept mtimes hours apart.
            if not np.array_equal(loaded['src_mtimes'],
                                  np.asarray(src_mtimes)):
                return None
            return loaded['amp'], loaded['phase'], loaded['prb']
    except Exception:
        return None


def _save_cached_scan(cache_path: Path, amp: NDArray, phase: NDArray,
                      prb: NDArray, src_mtimes: list[float]) -> None:
    try:
        np.savez(cache_path, amp=amp, phase=phase, prb=prb,
                 src_mtimes=src_mtimes)
    except OSError:
        pass


def _process_scan(
    obj_path: Path, prb_path: Path | None, sid: int, version: str,
    cache_dir: Path | None,
) -> tuple[NDArray, NDArray, NDArray]:
    """Load and process one scan, using the per-scan cache when valid.

    Returns (amplitude, phase, probe) for the scan.
    """
    cache_path = None
    src_mtimes = None
    if cache_dir is not None and prb_path is not None:
        cache_path = cache_dir / f'{sid}_{version}.npz'
        src_mtimes = [obj_path.stat().st_mtime, prb_path.stat().st_mtime]
        cached = _load_cached_scan(cache_path, src_mtimes)
        if cached is not None:
            return cached

    if prb_path is None:
        raise RuntimeError(
            f'Probe file is missing for SID {sid} ({version})')

    obj: NDArray = np.load(obj_path)
    prb: NDArray = np.load(prb_path)

    if obj.ndim == 3:
        obj = obj[0]

    if prb.ndim == 3:
        prb = prb[0]

    space = 15
    obj = np.fliplr(np.rot90(obj))
    prb = np.fliplr(np.rot90(prb))
    prb_sz = np.shape(prb)
    obj_sz = np.shape(obj)
    obj_c = obj[
        int(prb_sz[0] / 2) + space: obj_sz[0] - int(prb_sz[0] / 2) - space,
        int(prb_sz[1] / 2) + space: obj_sz[1] - int(prb_sz[1] / 2) - space,
    ]
    obj_c_arg = np.angle(obj_c)
    obj_c_amp = np.abs(obj_c)
    obj_c_arg = _remove_background(obj_c_arg)
    objectoutput = obj_c_amp * np.exp((0 + 1j) * obj_c_arg)
    obj_c_arg = np.angle(objectoutput)
    obj_c_amp = np.abs(obj_c)

    if cache_path is not None:
        _save_cached_scan(cache_path, obj_c_amp, obj_c_arg * -1, prb,
                          src_mtimes)

    return obj_c_amp, obj_c_arg * -1, prb


def _fit_func(p0: float, px1: float,
              py1: float) -> Callable[[NDArray, NDArray], NDArray]:
    return lambda x, y: p0 + x * px1 + py1 * y


def _remove_background(im: NDArray[np.floating]) -> NDArray[np.floating]:
    params = [0, 0, 0]

    def err_func(p: list[float]) -> NDArray:
        return np.ravel(_fit_func(*p)(*np.indices(im.shape)) - im)

    p, success = leastsq(err_func, params)
    return im - _fit_func(*p)(*np.indices(im.shape))


def _attempt_to_read_pixel_sizes(
    sid: int, version: str, ptycho_dir: str | Path,
) -> tuple[float, float] | None:
    path = locate_ptycho_hyan_file(sid, version, ptycho_dir)
    if path is None:
        print(
            f'Failed to locate config file for {sid} {version}\n'
            f'Pixel sizes will not be read',
            file=sys.stderr,
        )
        return None

    result = fetch_pixel_sizes_from_ptycho_hyan_file(path)
    if result is None:
        print(
            'Failed to obtain pixel sizes. Pixel sizes '
            'will not be applied to datasets.',
            file=sys.stderr,
        )
        return None

    print('Pixel sizes identified as:', result[0], result[1])
    return result[0], result[1]


def _stack_ptycho_data(
    version_list: list[str],
    sid_list: list[int],
    angle_list: list[float],
    ptycho_dir: str,
    rotate_datasets: bool = True,
) -> dict[str, object]:
    angle_list, sid_list, version_list = (
        zip(*sorted(zip(angle_list, sid_list, version_list)))
    )

    filespty_obj: list[Path] = []
    filespty_prb: list[Path | None] = []
    currentsidlist: list[tuple[int, int, float, str]] = []

    if len(version_list) == 1:
        version_list = np.repeat(version_list, len(sid_list))

    ptycho_dir_path = Path(ptycho_dir)

    pixel_size_x: float | None = None
    pixel_size_y: float | None = None
    for i, sid in tqdm(enumerate(sid_list[0: len(version_list)]),
                       desc="Loading Ptycho"):
        sid = int(sid)
        version = version_list[i]
        f_path = find_ptycho_file(sid, version, 'object', ptycho_dir_path)
        g_path = find_ptycho_file(sid, version, 'probe', ptycho_dir_path)
        if f_path is not None:
            filespty_obj.append(f_path)
            currentsidlist.append((i, sid, angle_list[i], version))
            filespty_prb.append(g_path)
            if i == 0:
                print('Attempting to read pixel sizes from the '
                      f'first scan ID: {sid}')
                result = _attempt_to_read_pixel_sizes(
                    sid, version, ptycho_dir_path)
                if result is not None:
                    pixel_size_x, pixel_size_y = result
        else:
            print(f"didn't find: {sid}")

    has_pixel_sizes = pixel_size_x is not None
    print(f"found: {len(filespty_obj)}")

    # Per-scan processing results are cached on disk, keyed by scan id,
    # version, and source file mtimes: when new scans arrive only they
    # are processed, instead of redoing the whole stack.
    cache_dir = _cache_dir_for(ptycho_dir)
    try:
        cache_dir.mkdir(parents=True, exist_ok=True)
    except OSError:
        cache_dir = None

    tempPtyobj: list[NDArray] = []
    tempPtyprb: list[NDArray] = []
    tempPtyamp: list[NDArray] = []
    for i in tqdm(range(len(filespty_obj)), desc="processing ptycho"):
        sid = currentsidlist[i][1]
        version = currentsidlist[i][3]
        amp, phase, prb = _process_scan(
            filespty_obj[i], filespty_prb[i], sid, version, cache_dir)
        tempPtyamp.append(amp)
        tempPtyobj.append(phase)
        tempPtyprb.append(prb)

    has_probes = True
    try:
        probes = np.asarray(tempPtyprb)
        probes_phase = np.angle(probes)
        probes_amp = np.abs(probes)
    except Exception as e:
        has_probes = False
        msg = (
            f'Failed to stack probes with error message: {e}\n'
            'Skipping over probe data...'
        )
        print(msg, file=sys.stderr)

    shapeslist = [i.shape for i in tempPtyobj]
    shapeslist = np.asarray(shapeslist)
    lmax = shapeslist[:, 0].max()
    wmax = shapeslist[:, 1].max()
    ptychodatanew = np.zeros((len(tempPtyobj), int(lmax), int(wmax)))
    ampdatanew = np.zeros((len(tempPtyobj), int(lmax), int(wmax)))
    for n, i in tqdm(enumerate(tempPtyobj), desc="correcting shape"):
        lerr = int(lmax - i.shape[0])
        werr = int(wmax - i.shape[1])
        # Center the smaller image; the remainder of an odd difference
        # goes after, so the total padding always reaches the max shape
        # (half-and-half padding dropped a row/column for odd
        # differences and made the stack assignment fail).
        pad = ((lerr // 2, lerr - lerr // 2), (werr // 2, werr - werr // 2))
        ptychodatanew[n, :, :] = np.pad(i, pad)
        ampdatanew[n, :, :] = np.pad(tempPtyamp[n], pad)

    arrays: dict[str, NDArray] = {
        'Phase': ptychodatanew,
        'Amplitude': ampdatanew,
    }
    if has_probes:
        arrays = {
            **arrays,
            'Probes Phase': probes_phase,
            'Probes Amplitude': probes_amp,
        }

    for key, array in arrays.items():
        if rotate_datasets:
            array = _rotate_stack_minus_90(array)

        array = array.swapaxes(0, 2)
        arrays[key] = array

    return {
        'arrays': arrays,
        'tilt_angles': np.array([x[2] for x in currentsidlist]),
        'scan_ids': np.array([x[1] for x in currentsidlist], dtype=np.int32),
        'has_probes': has_probes,
        'has_pixel_sizes': has_pixel_sizes,
        'pixel_size_x': pixel_size_x,
        'pixel_size_y': pixel_size_y,
        'info_rows': currentsidlist,
    }


def _write_ptycho_info_file(
    output_path: str,
    info_rows: list[tuple[int, int, float, str]],
) -> None:
    if Path(output_path).suffix.lower() == '.csv':
        # Write the scan list CSV format, whose columns match the
        # pyxrf-utils log file so that either the ptycho or the pyxrf
        # dialog can load it back in.
        with open(Path(output_path), 'w', newline='') as wf:
            writer = csv.writer(wf)
            writer.writerow(['Scan ID', 'Theta', 'Use', 'Version'])
            for row in info_rows:
                writer.writerow([row[1], f'{row[2]:.3f}', 1, row[3]])
        return

    currentsidlist_str: list[list[str]] = []
    for row in info_rows:
        this_row: list[str] = []
        for entry in row:
            if isinstance(entry, float):
                s = f'{entry:.3f}'
            else:
                s = str(entry)
            this_row.append(s)
        currentsidlist_str.append(this_row)

    col_delim = ' '
    headers = ['Angle', 'SID', 'Version']
    index_order = [2, 1, 3]
    col_width = 10
    with open(Path(output_path), 'w') as wf:
        header_str = col_delim.join([f'{x:>{col_width}}' for x in headers])
        header_str = '#' + header_str[1:]
        wf.write(header_str + '\n')
        for row_str_list in currentsidlist_str:
            row_str = col_delim.join([
                f'{row_str_list[idx]:>{col_width}}' for idx in index_order
            ])
            wf.write(row_str + '\n')


class PtychoSource(tomviz.nodes.SourceNode):

    def should_auto_execute(self, **parameters) -> bool:
        # Watch the reconstruction directory: a new scan showing up, or
        # an existing reconstruction being rewritten, changes the
        # fingerprint and requests a re-run. The first check only
        # records the current state, so enabling periodic execution
        # does not immediately reload data that is already in.
        ptycho_dir = parameters.get('ptycho_dir', '')
        if not ptycho_dir or not Path(ptycho_dir).is_dir():
            return False

        fingerprint = _dir_fingerprint(ptycho_dir)
        previous = self.state.get('dir_fingerprint')
        self.state['dir_fingerprint'] = fingerprint
        changed = previous is not None and fingerprint != previous

        if changed:
            # New scans must also enter the sid/version/angle parameter
            # lists, or the re-run would rebuild the same stack; the
            # write-back keeps the dialog and saved state in step too.
            # Best-effort: a corrupt config file must not eat the
            # re-run this change should trigger.
            try:
                self._absorb_new_scans(parameters)
            except Exception as exc:
                print(f'Warning: could not absorb new scans: {exc}',
                      file=sys.stderr)

        return changed

    def _absorb_new_scans(self, parameters: dict) -> None:
        sids: list[int] = json.loads(parameters.get('sid_list', '[]'))
        versions: list[str] = json.loads(
            parameters.get('version_list', '[]'))
        angles: list[float] = json.loads(parameters.get('angle_list', '[]'))

        # sid_list holds only the scans marked "Use"; ui_state's full
        # table also knows the deliberately deselected ones. Only scans
        # absent from both are new.
        known = set(sids)
        try:
            full_sids = json.loads(
                parameters.get('ui_state', '{}')).get('full_sid_list')
            if isinstance(full_sids, list):
                known.update(full_sids)
        except ValueError:
            pass

        on_disk = _discover_scans(parameters.get('ptycho_dir', ''))
        new_sids = sorted(set(on_disk) - known)
        if not new_sids:
            return

        print('New scans detected:', new_sids)

        # A single version stands for every scan (the
        # _stack_ptycho_data broadcast); expand before appending.
        if len(versions) == 1 and len(sids) > 1:
            versions = versions * len(sids)

        triples = sorted(
            [(sid, version, angle)
             for sid, version, angle in zip(sids, versions, angles)] +
            [(sid, on_disk[sid][0], on_disk[sid][1]) for sid in new_sids])
        self.set_parameter(
            'sid_list', json.dumps([t[0] for t in triples]))
        self.set_parameter(
            'version_list', json.dumps([t[1] for t in triples]))
        self.set_parameter(
            'angle_list', json.dumps([t[2] for t in triples]))

        self._merge_ui_state(parameters, new_sids, on_disk)

    def _merge_ui_state(self, parameters: dict, new_sids: list[int],
                        on_disk: dict[int, tuple[str, float]]) -> None:
        # The dialog restores its table from ui_state's full lists, so
        # new scans merge in (marked used) with existing choices kept.
        try:
            ui_state = json.loads(parameters.get('ui_state', '{}'))
        except ValueError:
            return
        full_sids = ui_state.get('full_sid_list')
        full_versions = ui_state.get('full_version_list')
        full_use = ui_state.get('full_use_list')
        if not isinstance(full_sids, list) or not full_sids:
            # No table state recorded (parameter-only restore path);
            # sid_list alone drives the dialog then.
            return

        rows = {
            sid: (version, use)
            for sid, version, use in zip(full_sids, full_versions, full_use)
        }
        for sid in new_sids:
            if sid not in rows:
                rows[sid] = (on_disk[sid][0], True)

        # The dialog keeps its table sorted by SID
        merged = sorted(rows)
        ui_state['full_sid_list'] = merged
        ui_state['full_version_list'] = [rows[sid][0] for sid in merged]
        ui_state['full_use_list'] = [rows[sid][1] for sid in merged]
        self.set_parameter('ui_state', json.dumps(ui_state))

    def produce(self, ptycho_dir: str = '', output_info_file: str = '',
                rotate_datasets: bool = True, sid_list: str = '[]',
                version_list: str = '[]',
                angle_list: str = '[]',
                ui_state: str = '{}') -> dict[str, Dataset] | None:
        parsed_sids: list[int] = json.loads(sid_list)
        parsed_versions: list[str] = json.loads(version_list)
        parsed_angles: list[float] = json.loads(angle_list)

        if not parsed_sids:
            print('No scan IDs provided')
            return None

        result = _stack_ptycho_data(
            parsed_versions, parsed_sids, parsed_angles,
            ptycho_dir, rotate_datasets,
        )

        arrays = result['arrays']

        if output_info_file:
            Path(output_info_file).parent.mkdir(parents=True, exist_ok=True)
            _write_ptycho_info_file(output_info_file, result['info_rows'])

        # Build the object dataset (Phase + Amplitude)
        object_ds = self.create_dataset()
        object_ds.set_scalars('Phase', arrays['Phase'])
        object_ds.set_scalars('Amplitude', arrays['Amplitude'])
        object_ds.tilt_angles = result['tilt_angles']
        object_ds.scan_ids = result['scan_ids']
        object_ds.tilt_axis = 2
        if result['has_pixel_sizes']:
            object_ds.spacing = (
                result['pixel_size_x'], result['pixel_size_y'], 1)

        outputs: dict[str, Dataset] = {'object': object_ds}

        # Build the probe dataset if probes are available
        if result['has_probes']:
            probe_ds = self.create_dataset()
            probe_ds.set_scalars('Probes Phase', arrays['Probes Phase'])
            probe_ds.set_scalars('Probes Amplitude', arrays['Probes Amplitude'])
            probe_ds.tilt_angles = result['tilt_angles']
            probe_ds.scan_ids = result['scan_ids']
            probe_ds.tilt_axis = 2
            outputs['probe'] = probe_ds

        return outputs

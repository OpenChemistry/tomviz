"""Read scan metadata from PyXRF HDF5 files for the dialog scan table."""
from __future__ import annotations

import logging
import os

import h5py

logger = logging.getLogger(__name__)


def _expand_scan_range(scan_range: str) -> list[int]:
    if not scan_range or not scan_range.strip():
        return []
    ids: set[int] = set()
    for part in scan_range.split(','):
        part = part.strip()
        if not part:
            continue
        try:
            if ':' in part:
                pieces = part.split(':')
                start = int(pieces[0])
                stop = int(pieces[1])
                stride = int(pieces[2]) if len(pieces) > 2 else 1
                if stride == 0:
                    continue
                ids.update(range(start, stop + 1, stride))
            else:
                ids.add(int(part))
        except (ValueError, IndexError):
            # Skip malformed segments rather than aborting the whole parse.
            continue
    return sorted(ids)


def _scan_ids_present(working_directory: str) -> list[int]:
    """Scan IDs for the ``scan2D_<id>.h5`` files in the directory."""
    import glob
    import re

    ids = []
    pattern = re.compile(r'^scan2D_(\d+)\.h5$')
    for path in glob.glob(os.path.join(working_directory, 'scan2D_*.h5')):
        match = pattern.match(os.path.basename(path))
        if match:
            ids.append(int(match.group(1)))
    return sorted(ids)


def read_scan_metadata(working_directory: str,
                       scan_range: str = '') -> list[dict]:
    """Return a list of scan metadata dicts from HDF5 files in *working_directory*.

    Each dict has keys: scan_id, theta, status, filename.

    Any ID in the range without an HDF5 file gets a ``"missing"`` status
    entry, and a file that exists but cannot be read gets a ``"fail"`` entry.
    The list is sorted by scan ID.

    If *scan_range* is empty, the table is populated from the
    ``scan2D_<id>.h5`` files already present in the directory, so a
    directory of downloaded scans shows its contents without requiring
    the range to be typed first (matching the ptycho dialog). Only files
    matching that exact naming pattern are opened. A range that is given
    but names no scans (a typo, say) lists nothing rather than every file.
    """
    wd = working_directory

    if scan_range and scan_range.strip():
        expected_ids = _expand_scan_range(scan_range)
    else:
        expected_ids = _scan_ids_present(wd)
    if not expected_ids:
        return []

    # Only read the files for the scan IDs the user asked for, so out-of-range
    # (and possibly unreadable) files are never opened.
    found: dict[int, dict] = {}
    failed_files: dict[int, str] = {}
    for sid in expected_ids:
        path = os.path.join(wd, f'scan2D_{sid}.h5')
        if not os.path.isfile(path):
            continue
        try:
            with h5py.File(path, 'r') as f:
                md = f['xrfmap/scan_metadata']
                scan_id = int(md.attrs['scan_id'])
                theta = float(md.attrs['param_theta'])
                theta_units = md.attrs.get('param_theta_units', 'deg')
                if theta_units == 'mdeg':
                    theta /= 1000.0
                theta = round(theta, 3)
                status = str(md.attrs.get('scan_exit_status', ''))
                found[scan_id] = {
                    'scan_id': scan_id,
                    'theta': theta,
                    'status': status if status else 'success',
                    'filename': os.path.basename(path),
                }
        except Exception as e:
            failed_files[sid] = os.path.basename(path)
            logger.warning('Failed to read scan %s from %s: %s: %s', sid, path,
                           type(e).__name__, e)

    results = []
    for sid in expected_ids:
        if sid in found:
            results.append(found[sid])
        elif sid in failed_files:
            results.append({
                'scan_id': sid,
                'theta': 0.0,
                'status': 'fail',
                'filename': failed_files[sid],
            })
        else:
            results.append({
                'scan_id': sid,
                'theta': 0.0,
                'status': 'missing',
                'filename': '',
            })

    results.sort(key=lambda r: r['scan_id'])
    return results

"""A fake pyxrf-utils for testing the PyXRF source without PyXRF.

    raw-scan <dir> <scan id> <theta>
        Write an unfitted scan2D_<id>.h5, as a batch download would.
    process-projections <dir> --range <range> ... -o <output dir>
        Fit by writing into the scan files, like xrf_tomo does, then
        write tomo.h5 sorted by angle.
"""
import json
import sys
from pathlib import Path

import h5py
import numpy as np


def raw_scan(directory: Path, sid: int, theta: float) -> None:
    with h5py.File(Path(directory) / f'scan2D_{sid}.h5', 'w') as f:
        md = f.create_group('xrfmap/scan_metadata')
        md.attrs['param_theta'] = theta
        md.attrs['param_input'] = [0.0, 0.4, 4, 0.0, 0.3, 3]
        f.create_group('xrfmap/detsum')
        f['raw'] = np.full((1, 3, 4), float(sid))


def process_projections(args: list[str]) -> None:
    wd = Path(args[0])
    skip_processed = '-s' in args
    rest = [a for a in args[1:] if a != '-s']
    opts = dict(zip(rest[::2], rest[1::2]))

    ids = set()
    for part in opts['--range'].split(','):
        start, _, stop = part.strip().partition(':')
        ids.update(range(int(start), int(stop or start) + 1))
    ids -= set(json.loads(opts.get('--skip', '[]')))

    scans = []
    for path in [wd / f'scan2D_{sid}.h5' for sid in sorted(ids)]:
        if not path.exists():
            continue
        with h5py.File(path, 'a') as f:
            detsum = f['xrfmap/detsum']
            if 'xrf_fit' not in detsum or not skip_processed:
                detsum.pop('xrf_fit', None)
                detsum['xrf_fit'] = f['raw'][()]
            theta = f['xrfmap/scan_metadata'].attrs['param_theta']
            scans.append((theta, detsum['xrf_fit'][()]))

    scans.sort(key=lambda s: s[0])
    with h5py.File(Path(opts['-o']) / 'tomo.h5', 'w') as f:
        f['reconstruction/fitting/data'] = np.stack([s[1] for s in scans])
        f['reconstruction/fitting/elements'] = [b'Fe_K']
        f['exchange/theta'] = [s[0] for s in scans]


if __name__ == '__main__':
    command, args = sys.argv[1], sys.argv[2:]
    if command == 'raw-scan':
        raw_scan(Path(args[0]), int(args[1]), float(args[2]))
    elif command == 'process-projections':
        process_projections(args)
    else:
        sys.exit(f'unsupported command: {command}')

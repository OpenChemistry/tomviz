#!/usr/bin/env python3
"""Simulate a PyXRF acquisition: periodically add scan files to a directory.

Every ``--interval`` seconds a new ``scan2D_<sid>.h5`` appears in the
output directory with the ``xrfmap`` layout the PyXRF tooling reads, and
``tomo.h5`` in the same directory is reassembled to include the new
projection. Point a tomviz PyXRF source's working directory here (leave
the scan range empty so it reads tomo.h5 directly), enable Periodic
Execution on the node, and the volume should grow as scans "arrive".

Some scans are emitted with a slightly different pixel shape to exercise
the auto-padding path.

Example:
    python simulate_pyxrf_stream.py /tmp/pyxrf-sim --interval 5 --count 30
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import h5py
import numpy as np

ELEMENTS = ['Fe_K', 'Ni_K', 'Ca_K']
BASE_SHAPE = (48, 64)  # rows, columns


def _phantom(theta_deg: float, shape: tuple[int, int],
             rng: np.random.Generator) -> np.ndarray:
    """A fake projection: two blobs whose separation follows the angle."""
    h, w = shape
    y, x = np.mgrid[0:h, 0:w]
    cx = w / 2 + (w / 4) * np.cos(np.radians(theta_deg))
    cy = h / 2
    blob1 = np.exp(-(((x - cx) / 6) ** 2 + ((y - cy) / 8) ** 2))
    cx2 = w / 2 - (w / 5) * np.cos(np.radians(theta_deg))
    blob2 = 0.6 * np.exp(-(((x - cx2) / 4) ** 2 + ((y - cy - 6) / 5) ** 2))
    return blob1 + blob2 + rng.normal(0, 0.01, shape)


def write_scan(directory: Path, sid: int, theta: float,
               shape: tuple[int, int], rng: np.random.Generator) -> None:
    h, w = shape
    path = directory / f'scan2D_{sid}.h5'
    # Write to a temp name then rename, so a reader never sees a
    # half-written file (matches rebuild_tomo and the ptycho simulator).
    tmp_path = directory / f'.partial_scan2D_{sid}.h5'
    with h5py.File(tmp_path, 'w') as f:
        md = f.create_group('xrfmap/scan_metadata')
        md.attrs['scan_id'] = sid
        md.attrs['scan_uid'] = f'sim-{sid:08d}'
        md.attrs['scan_time_start'] = time.strftime(
            '%Y-%m-%dT%H:%M:%S+00:00', time.gmtime())
        md.attrs['param_theta'] = theta
        md.attrs['param_theta_units'] = 'deg'
        md.attrs['scan_exit_status'] = 'success'
        # x_start, x_stop, num_x, y_start, y_stop, num_y
        md.attrs['param_input'] = [0.0, w * 0.1, w, 0.0, h * 0.1, h]

        fit = np.stack([
            (i + 1) * _phantom(theta, shape, rng) for i in range(len(ELEMENTS))
        ])
        detsum = f.create_group('xrfmap/detsum')
        detsum.create_dataset('xrf_fit', data=fit)
        detsum.create_dataset(
            'xrf_fit_name', data=np.array([e.encode() for e in ELEMENTS]))

        pos = np.stack([np.mgrid[0:h, 0:w][0].astype(float),
                        np.mgrid[0:h, 0:w][1].astype(float)])
        f.create_group('xrfmap/positions').create_dataset('pos', data=pos)

        scalers = f.create_group('xrfmap/scalers')
        scalers.create_dataset('name', data=np.array([b'sclr1_ch4']))
        scalers.create_dataset(
            'val', data=np.ones((h, w, 1)))

    tmp_path.replace(path)


def rebuild_tomo(directory: Path) -> None:
    """Assemble tomo.h5 from every scan present, padding to the max shape."""
    scans = []
    for path in sorted(directory.glob('scan2D_*.h5')):
        with h5py.File(path, 'r') as f:
            theta = float(f['xrfmap/scan_metadata'].attrs['param_theta'])
            fit = f['xrfmap/detsum/xrf_fit'][()]
            scans.append((theta, fit))
    scans.sort(key=lambda item: item[0])

    hmax = max(fit.shape[1] for _, fit in scans)
    wmax = max(fit.shape[2] for _, fit in scans)
    data = np.zeros((len(scans), scans[0][1].shape[0], hmax, wmax))
    for i, (_, fit) in enumerate(scans):
        pad = ((0, 0), (hmax - fit.shape[1], 0), (wmax - fit.shape[2], 0))
        data[i] = np.pad(fit, pad)

    # Write to a temp name then replace, so a reader never sees a
    # half-written file.
    tmp = directory / 'tomo.h5.partial'
    with h5py.File(tmp, 'w') as f:
        f.create_dataset('reconstruction/fitting/data', data=data)
        f.create_dataset(
            'reconstruction/fitting/elements',
            data=np.array([e.encode() for e in ELEMENTS]))
        f.create_dataset(
            'exchange/theta', data=np.array([t for t, _ in scans]))
    tmp.replace(directory / 'tomo.h5')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', help='output directory')
    parser.add_argument('--interval', type=float, default=5.0,
                        help='seconds between scans (default 5)')
    parser.add_argument('--count', type=int, default=37,
                        help='number of scans to emit (default 37)')
    parser.add_argument('--start-sid', type=int, default=100001)
    parser.add_argument('--seed', type=int, default=0)
    args = parser.parse_args()

    directory = Path(args.directory)
    directory.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)

    thetas = np.linspace(-90, 90, args.count)
    for i in range(args.count):
        sid = args.start_sid + i
        shape = BASE_SHAPE
        # Every fifth scan arrives with fewer pixels, like the real
        # beamline occasionally produces.
        if i % 5 == 4:
            shape = (BASE_SHAPE[0] - 2, BASE_SHAPE[1] - 3)
        write_scan(directory, sid, float(thetas[i]), shape, rng)
        rebuild_tomo(directory)
        print(f'[{i + 1}/{args.count}] scan {sid} at '
              f'theta={thetas[i]:.1f} shape={shape}', flush=True)
        if i + 1 < args.count:
            time.sleep(args.interval)
    print('done')


if __name__ == '__main__':
    main()

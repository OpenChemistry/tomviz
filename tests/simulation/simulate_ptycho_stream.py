#!/usr/bin/env python3
"""Simulate a ptychography reconstruction stream.

Every ``--interval`` seconds a new scan's reconstruction appears in the
output directory using the layout the tomviz ptycho source reads:

    S<sid>/<version>/recon_data/recon_<sid>_<version>_object_ave.npy
    S<sid>/<version>/recon_data/recon_<sid>_<version>_probe_ave.npy

Point a tomviz Ptycho source's directory here and enable Periodic
Execution on the node; each new scan changes the directory fingerprint
and triggers a re-run. The scan IDs and angles the widget needs are
printed as they are emitted (and written to ``angles.txt``).

Some scans are emitted with a slightly different object shape to
exercise the stack-padding path.

Example:
    python simulate_ptycho_stream.py /tmp/ptycho-sim --interval 5 --count 20
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np

VERSION = 't1'
BASE_SHAPE = (140, 150)
PROBE_SHAPE = (32, 32)


def _object(theta_deg: float, shape: tuple[int, int],
            rng: np.random.Generator) -> np.ndarray:
    h, w = shape
    y, x = np.mgrid[0:h, 0:w]
    cx = w / 2 + (w / 6) * np.cos(np.radians(theta_deg))
    amp = 1.0 - 0.5 * np.exp(-(((x - cx) / 12) ** 2 + ((y - h / 2) / 15) ** 2))
    phase = 0.8 * np.exp(-(((x - cx) / 10) ** 2 + ((y - h / 2) / 12) ** 2))
    phase += rng.normal(0, 0.01, shape)
    return (amp * np.exp(1j * phase)).astype(np.complex64)


def write_scan(root: Path, sid: int, theta: float,
               shape: tuple[int, int], rng: np.random.Generator) -> None:
    recon_dir = root / f'S{sid}' / VERSION / 'recon_data'
    recon_dir.mkdir(parents=True, exist_ok=True)
    base = f'recon_{sid}_{VERSION}'

    obj = _object(theta, shape, rng)
    probe = (rng.normal(size=PROBE_SHAPE) +
             1j * rng.normal(size=PROBE_SHAPE)).astype(np.complex64)

    # The per-scan config file real reconstructions carry: tomviz reads
    # the tilt angle (and pixel sizes) from it, both in the dialog and
    # when absorbing new scans during periodic execution.
    config = recon_dir / f'{sid}_{VERSION}.txt'
    config.write_text(
        f'angle = {theta}\n'
        'lambda_nm = 0.1\n'
        'z_m = 2.0\n'
        f'nx = {shape[1]}\n'
        f'ny = {shape[0]}\n'
        'ccd_pixel_um = 55\n')

    # Write to temp names then rename, so a reader never sees a
    # half-written array. The temp name keeps the .npy suffix because
    # np.save appends one to any other extension.
    for name, array in ((f'{base}_object_ave.npy', obj),
                        (f'{base}_probe_ave.npy', probe)):
        tmp = recon_dir / ('.partial_' + name)
        np.save(tmp, array)
        tmp.replace(recon_dir / name)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', help='output directory')
    parser.add_argument('--interval', type=float, default=5.0,
                        help='seconds between scans (default 5)')
    parser.add_argument('--count', type=int, default=20,
                        help='number of scans to emit (default 20)')
    parser.add_argument('--start-sid', type=int, default=50001)
    parser.add_argument('--seed', type=int, default=0)
    args = parser.parse_args()

    root = Path(args.directory)
    root.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)

    thetas = np.linspace(-60, 60, args.count)
    angles_file = root / 'angles.txt'
    with open(angles_file, 'w') as f:
        f.write('# sid version angle\n')

    for i in range(args.count):
        sid = args.start_sid + i
        shape = BASE_SHAPE
        # Every fourth scan reconstructs with a slightly different crop.
        if i % 4 == 3:
            shape = (BASE_SHAPE[0] - 5, BASE_SHAPE[1] + 4)
        write_scan(root, sid, float(thetas[i]), shape, rng)
        with open(angles_file, 'a') as f:
            f.write(f'{sid} {VERSION} {thetas[i]:.3f}\n')
        print(f'[{i + 1}/{args.count}] scan {sid} at '
              f'theta={thetas[i]:.1f} shape={shape}', flush=True)
        if i + 1 < args.count:
            time.sleep(args.interval)
    print(f'done; sid/angle list in {angles_file}')


if __name__ == '__main__':
    main()

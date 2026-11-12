from __future__ import annotations

from pathlib import Path

from pyxrf.api_dev import make_hdf
import pyxrf.model.load_data_from_db

if pyxrf.model.load_data_from_db.db is None:
    pyxrf.model.load_data_from_db.catalog_info.set_name('HXN')

    from hxntools.CompositeBroker import db
    pyxrf.model.load_data_from_db.db = db


def _modified(path: Path) -> int | None:
    """Modification time of *path* in ns, or None if it doesn't exist."""
    try:
        return path.stat().st_mtime_ns
    except FileNotFoundError:
        return None


def make_hdf5(
    scan_ids: list[int],
    working_directory: Path,
    force: bool = False,
) -> None:
    """Download HDF5 files for the given scan IDs.

    For each ID, checks whether ``scan2D_{id}.h5`` already exists.
    Skips the download if the file is present and *force* is False.
    Calls ``make_hdf`` with start==end so failures are silently skipped.
    """
    wd = Path(working_directory)
    wd.mkdir(parents=True, exist_ok=True)

    for sid in scan_ids:
        target = wd / f'scan2D_{sid}.h5'
        if target.exists() and not force:
            print(f'Scan {sid}: {target.name} already exists, skipping',
                  flush=True)
            continue

        print(f'Downloading scan {sid} ...', flush=True)
        before = _modified(target)
        try:
            make_hdf(
                sid, sid,
                wd=str(wd),
                file_overwrite_existing=force,
            )
        except KeyboardInterrupt:
            # Canceled mid-download. A partial file would be skipped as
            # already downloaded on the next run. An earlier complete one
            # that pyxrf hadn't started rewriting yet is kept.
            if _modified(target) != before:
                target.unlink(missing_ok=True)
            raise
        except Exception as exc:
            print(f'Warning: scan {sid} failed to download: {exc}',
                  flush=True)

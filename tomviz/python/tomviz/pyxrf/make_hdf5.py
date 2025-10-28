from pathlib import Path
import shutil

import h5py
from pyxrf.api_dev import make_hdf
import pyxrf.model.load_data_from_db
from xrf_tomo import create_log_file


if pyxrf.model.load_data_from_db.db is None:
    # Manually force the database to be HXN if it was not identified correctly
    pyxrf.model.load_data_from_db.catalog_info.set_name('HXN')

    from hxntools.CompositeBroker import db
    pyxrf.model.load_data_from_db.db = db


def make_hdf5(start_scan, stop_scan, working_directory, successful_scans_only,
              log_file_name):

    kwargs = {
        'start': start_scan,
        'end': stop_scan,
        'wd': working_directory,
        'successful_scans_only': successful_scans_only,
    }
    make_hdf(**kwargs)

    # Move the invalid files into an "invalid" subdirectory, or else
    # the log file will fail to be created.
    invalid_dir = Path(working_directory) / 'invalid'
    for filepath in Path(working_directory).glob('scan2D_*.h5'):
        if not _hdf5_is_valid(filepath):
            invalid_dir.mkdir(parents=True, exist_ok=True)
            shutil.move(filepath, invalid_dir)

    kwargs = {
        'fn_log': log_file_name,
        'wd': working_directory,
    }
    create_log_file(**kwargs)


def _hdf5_is_valid(path: Path) -> bool:
    try:
        with h5py.File(path) as f:
            return 'param_input' in f['xrfmap/scan_metadata'].attrs
    except Exception:
        return False

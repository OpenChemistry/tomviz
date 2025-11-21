from pathlib import Path

from pyxrf.api_dev import make_hdf
import pyxrf.model.load_data_from_db

from .create_log_file import create_log_file

if pyxrf.model.load_data_from_db.db is None:
    # Manually force the database to be HXN if it was not identified correctly
    pyxrf.model.load_data_from_db.catalog_info.set_name('HXN')

    from hxntools.CompositeBroker import db
    pyxrf.model.load_data_from_db.db = db


def make_hdf5(
    start_scan: int,
    stop_scan: int,
    working_directory: Path,
    successful_scans_only: bool,
    log_file_name: Path,
):
    kwargs = {
        'start': start_scan,
        'end': stop_scan,
        'wd': working_directory,
        'successful_scans_only': successful_scans_only,
    }
    make_hdf(**kwargs)

    kwargs = {
        'log_file_name': log_file_name,
        'working_directory': working_directory,
    }
    create_log_file(**kwargs)

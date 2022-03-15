from pyxrf.api_dev import make_hdf

from xrf_tomo import create_log_file


def make_hdf5(start_scan, stop_scan, working_directory, successful_scans_only,
              log_file_name):

    kwargs = {
        'start': start_scan,
        'end': stop_scan,
        'wd': working_directory,
        'successful_scans_only': successful_scans_only,
    }
    make_hdf(**kwargs)

    kwargs = {
        'fn_log': log_file_name,
        'wd': working_directory,
    }
    create_log_file(**kwargs)

from pathlib import Path
import shutil

import h5py
from xrf_tomo import process_proj, make_single_hdf

# sys.stderr has been replaced by vtkPythonStdStreamCaptureHelper
# As such, it does not have an "isatty()" function. Therefore, turn off
# the tty check in the progress library.
try:
    import progress
    progress.Infinite.check_tty = False
except Exception:
    # Maybe a new version of pyxrf removed dependency on progress...
    # Or maybe progress refactored. Either way, let it go...
    pass


def ic_names(working_directory):
    # Find any HDF5 file in the working directory and grab the ic names
    files = [x for x in Path(working_directory).iterdir() if x.suffix == '.h5']
    if not files:
        raise Exception(f'No .h5 files found in "{working_directory}"')

    first_file = files[0]

    with h5py.File(first_file, 'r') as rf:
        names = rf["xrfmap"]["scalers"]["name"]
        names = [x.decode() for x in names]
        return names


def process_projections(working_directory, parameters_file_name, log_file_name,
                        ic_name, skip_processed, output_directory):
    kwargs = {
        'wd': working_directory,
        'fn_param': parameters_file_name,
        'fn_log': log_file_name,
        'ic_name': ic_name,
        'skip_processed': skip_processed,
    }
    process_proj(**kwargs)

    kwargs = {
        'fn': 'tomo.h5',
        'fn_log': log_file_name,
        'wd_src': working_directory,
        'wd_dest': output_directory,
        'ic_name': ic_name,
    }
    make_single_hdf(**kwargs)

    # Copy the csv file into the output directory
    shutil.copyfile(Path(working_directory) / log_file_name,
                    Path(output_directory) / log_file_name)

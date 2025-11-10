import os
import multiprocessing
from pathlib import Path
import shutil
import sys

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
    # FIXME: this shouldn't be necessary. When we get time, figure out why
    # we have to specify these paths. Currently, if we do not, then the
    # multiprocessing will encounter errors about missing standard python
    # libraries.
    fix_python_paths()

    # Because we are running an embedded tomviz, multiprocessing can't
    # find the python executable. Therefore, we need to try to find it
    # automatically and tell multiprocessing where it is.
    python_exec = find_python_executable()
    multiprocessing.set_executable(python_exec)

    kwargs = {
        'wd': working_directory,
        'fn_param': parameters_file_name,
        'fn_log': log_file_name,
        'ic_name': ic_name,
        'skip_processed': skip_processed,
    }
    process_proj(**kwargs)

    # Ensure the output directory exists
    Path(output_directory).mkdir(parents=True, exist_ok=True)
    kwargs = {
        'fn': 'tomo.h5',
        'fn_log': log_file_name,
        'wd_src': working_directory,
        'wd_dest': output_directory,
        'ic_name': ic_name,
    }
    make_single_hdf(**kwargs)

    # Copy the csv file into the output directory
    log_file_path = Path(log_file_name)
    if not log_file_path.is_absolute():
        log_file_path = Path(working_directory).resolve() / log_file_path

    output_file_path = Path(output_directory).resolve() / log_file_path.name
    if log_file_path != output_file_path:
        # Copy the csv file into the output directory
        shutil.copyfile(log_file_path, output_file_path)


def fix_python_paths():
    # FIXME: this shouldn't be necessary, but otherwise, we get errors
    # indicating that python standard libraries couldn't be found.
    # It seems the embedded python in our conda package doesn't set up
    # its sys.path correctly.
    python_path_var = os.environ.get('PYTHONPATH', '')
    python_paths = python_path_var.split(':') if python_path_var else []
    python_lib_path = f'lib/python3.{sys.version_info.minor}'

    conda_path = Path(os.environ.get('CONDA_PREFIX', ''))
    need_to_add = [
        conda_path / f'{python_lib_path}/site-packages',
        conda_path / f'{python_lib_path}/lib-dynload',
    ]

    for path in need_to_add:
        if str(path) not in python_paths:
            python_paths.append(str(path))

    os.environ['PYTHONPATH'] = ':'.join(python_paths)


def find_python_executable():
    # Since we are in tomviz, sys.executable does not work
    failure_msg = (
        'Could not find python executable for multiprocessing. '
        'Please set it to the environment variable "TOMVIZ_PYTHON_EXECUTABLE" '
        'and restart tomviz.'
    )

    # First, check if the user set it via an environment variable
    path = os.environ.get('TOMVIZ_PYTHON_EXECUTABLE')
    if path and Path(path).exists():
        return path

    # Next, try to find it via the conda prefix
    conda_path = os.environ.get('CONDA_PREFIX')
    if conda_path is None:
        raise Exception(failure_msg)

    try_list = [
        Path(conda_path) / 'bin' / 'python',
        Path(conda_path) / 'bin' / 'python3',
        Path(conda_path) / 'bin' / 'python.exe',
    ]
    for path in try_list:
        if path.exists():
            return str(path)

    raise Exception(failure_msg)

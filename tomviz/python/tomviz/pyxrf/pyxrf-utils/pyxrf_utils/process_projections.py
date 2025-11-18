from pathlib import Path
import shutil

from xrf_tomo import process_proj, make_single_hdf


def process_projections(
    working_directory: str,
    parameters_file_name: str,
    log_file_name: str,
    ic_name: str,
    skip_processed: bool,
    output_directory: str,
):
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

from pathlib import Path

import h5py

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

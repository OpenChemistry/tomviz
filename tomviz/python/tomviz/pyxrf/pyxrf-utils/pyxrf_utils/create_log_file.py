from pathlib import Path
import glob
import os
import sys

import h5py
import numpy as np
import pandas as pd
import time

from pyxrf.core.utils import convert_time_from_nexus_string


def create_log_file(log_file_name: str, working_directory: str,
                    sid_selection: str | None = None,
                    hdf5_glob_pattern: str = "scan2D_*.h5",
                    skip_invalid: bool = True):
    """
    Create log file ``fn`` based on the files contained in ``wd``. If ``fn``
    is a relative path, it is assumed that the root is in ``wd``.
    """
    wd = Path(working_directory).resolve()
    fn_log = Path(log_file_name)
    if not fn_log.is_absolute():
        fn_log = wd / fn_log

    selected_sids = []
    if sid_selection is not None:
        sid_strings = sid_selection.split(',')
        for this_str in sid_strings:
            if ':' in this_str:
                this_slice = slice(
                    *(int(s) if s else None for s in this_str.split(':'))
                )
                selected_sids += np.r_[this_slice].tolist()
            else:
                selected_sids.append(int(this_str))

    # Create the list of HDF5 files
    hdf5_list = glob.glob(os.path.join(wd, hdf5_glob_pattern))

    mdata_keys = [
        "scan_time_start",
        "scan_id",
        "param_theta",
        "param_theta_units",
        "param_input",
        "scan_uid",
        "scan_exit_status",
    ]
    hdf5_mdata = {}

    for hdf5_fn in hdf5_list:
        try:
            mdata, mdata_selected = {}, {}
            with h5py.File(hdf5_fn, "r") as f:
                # Retrieve metadata if it exists
                metadata = f["xrfmap/scan_metadata"]
                for key, value in metadata.attrs.items():
                    # Convert ndarrays to lists
                    # (they were lists before they were saved)
                    if isinstance(value, np.ndarray):
                        value = list(value)
                    mdata[key] = value
            mdata_available_keys = set(mdata.keys())
            mdata_missing_keys = set(mdata_keys) - mdata_available_keys
            if mdata_missing_keys:
                raise IndexError(
                    "The following metadata keys are missing in file "
                    f"'{hdf5_fn}': {mdata_missing_keys}. "
                    "Log file can not be created. Make sure that the files "
                    "are created using recent version of PyXRF"
                )

            if sid_selection is not None:
                sid = int(mdata['scan_id'])
                if sid not in selected_sids:
                    # Skip over ones that were not selected.
                    continue

            mdata_selected = {_: mdata[_] for _ in mdata_keys}
            if mdata_selected["param_theta_units"] == "mdeg":
                mdata_selected["param_theta"] /= 1000
                mdata_selected["param_theta_units"] = "deg"

            hdf5_mdata[os.path.basename(hdf5_fn)] = mdata_selected

        except Exception as ex:
            msg = f"Failed to load metadata from '{hdf5_fn}': {ex}"
            if not skip_invalid:
                raise IOError(msg) from ex

            print(msg, file=sys.stderr)
            print(f"Skipping '{hdf5_fn}'", file=sys.stderr)

    # List of file names sorted by the angle 'theta'
    hdf5_names_sorted = sorted(
        hdf5_mdata,
        key=lambda _: hdf5_mdata[_]["param_theta"],
    )

    column_labels = [
        "Start Time",
        "Scan ID",
        "Theta",
        "Use",
        "Filename",
        "X Start",
        "X Stop",
        "Num X",
        "Y Start",
        "Y Stop",
        "Num Y",
        "Dwell",
        "UID",
        "Status",
        "Version",
    ]
    hdf5_mdata_sorted = []
    for hdf5_fn in hdf5_names_sorted:
        md = hdf5_mdata[hdf5_fn]
        hdf5_mdata_sorted.append(
            [
                time.strftime(
                    "%a %b %d %H:%M:%S %Y",
                    convert_time_from_nexus_string(md["scan_time_start"]),
                ),
                md["scan_id"],
                np.round(md["param_theta"], 3),
                "1",
                hdf5_fn,
                *[np.round(_, 3) for _ in md["param_input"][0:7]],
                md["scan_uid"],
                md["scan_exit_status"],
                "t1",
            ]
        )
    df = pd.DataFrame(data=hdf5_mdata_sorted, columns=column_labels)

    os.makedirs(os.path.dirname(fn_log), exist_ok=True)
    df.to_csv(fn_log, sep=",", index=False)
    print(f"Log file '{fn_log}' was successfully created.")

from __future__ import annotations

from pathlib import Path
import shutil

import h5py
import numpy as np
from xrf_tomo import process_proj, make_single_hdf
from xrf_tomo.xrf_tomo_workflow import read_log_file

from .create_log_file import create_log_file


def _used_files_sorted_by_theta(
    fn_log: str, wd_src: str,
) -> tuple[list[str], list[float]]:
    log = read_log_file(fn_log, wd=wd_src)
    log = log[log["Use"]]
    log = log.sort_values(by=["Theta"])
    return list(log["Filename"].values), list(log["Theta"].values)


def _fit_shape(path: Path) -> tuple[int, ...]:
    with h5py.File(path, "r") as f:
        return tuple(f["xrfmap"]["detsum"]["xrf_fit"].shape)


def _make_single_hdf_padded(
    fn: Path,
    fn_log: str,
    wd_src: Path,
    ic_name: str,
) -> None:
    """Assemble tomo.h5 from scans whose pixel shapes differ.

    xrf_tomo.make_single_hdf sizes every dataset from the first scan, so
    a scan with more (or fewer) pixels than the first makes the stack
    assignment fail. Here every image is padded up to the largest shape
    across all scans, which is known up front, so earlier projections
    never need re-padding when a bigger one arrives. Padding goes before
    the data on each axis (top-left), following the convention of the
    beamline stacking scripts.
    """
    filenames, thetas = _used_files_sorted_by_theta(fn_log, str(wd_src))
    num = len(filenames)

    shapes = [_fit_shape(wd_src / name) for name in filenames]
    n_elements = {s[0] for s in shapes}
    if len(n_elements) != 1:
        raise RuntimeError(
            "Scans disagree on the number of fitted elements: "
            f"{sorted(n_elements)}. They were probably fitted with "
            "different parameter files; refit and try again.")
    hmax = max(s[1] for s in shapes)
    wmax = max(s[2] for s in shapes)
    print(f"Padding projections to {hmax} x {wmax}", flush=True)

    def padded(image: np.ndarray) -> np.ndarray:
        pad = ((hmax - image.shape[-2], 0), (wmax - image.shape[-1], 0))
        if image.ndim == 3:
            pad = ((0, 0), *pad)
        return np.pad(image, pad)

    with h5py.File(fn, "w") as f:
        for group in ("exchange", "measurement", "instrument", "provenance",
                      "reconstruction", "reconstruction/fitting",
                      "reconstruction/recon"):
            f.create_group(group)

        n_el = next(iter(n_elements))
        f_fit = f.create_dataset(
            "/reconstruction/fitting/data",
            shape=(num, n_el, hmax, wmax), compression="gzip")
        f_x = f.create_dataset(
            "/exchange/x", shape=(num, hmax, wmax), compression="gzip")
        f_y = f.create_dataset(
            "/exchange/y", shape=(num, hmax, wmax), compression="gzip")
        f_i0 = f.create_dataset(
            "/exchange/i0", shape=(num, hmax, wmax), compression="gzip")
        f.create_dataset("/exchange/theta", data=np.asarray(thetas))

        for i, name in enumerate(filenames):
            print(f"Collecting data...{i + 1:04d}/{num:04d} "
                  f"(file '{name}')", flush=True)
            with h5py.File(wd_src / name, "r") as tmp_f:
                detsum = tmp_f["xrfmap"]["detsum"]
                f_fit[i] = padded(detsum["xrf_fit"][()])
                f_x[i] = padded(tmp_f["xrfmap"]["positions"]["pos"][1])
                f_y[i] = padded(tmp_f["xrfmap"]["positions"]["pos"][0])

                scaler_names = [
                    _.decode() for _ in tmp_f["xrfmap"]["scalers"]["name"]
                ]
                try:
                    scaler_ind = scaler_names.index(ic_name)
                except ValueError:
                    raise RuntimeError(
                        f"Scaler '{ic_name}' is not found. Available "
                        f"scalers: {scaler_names}")
                f_i0[i] = padded(
                    tmp_f["xrfmap"]["scalers"]["val"][:, :, scaler_ind])

                if i == 0:
                    f.create_dataset(
                        "/reconstruction/fitting/elements",
                        data=np.array(detsum["xrf_fit_name"]))


def process_projections(
    scan_ids: list[int],
    working_directory: str,
    parameters_file_name: str,
    ic_name: str,
    output_directory: str,
    skip_processed: bool = True,
    csv_output: str = '',
) -> None:
    """Process XRF projections: generate CSV, fit, and assemble tomo.h5.

    A temporary CSV log is always created inside *working_directory* for
    ``xrf_tomo``.  If *csv_output* is set, the CSV is also copied there.
    """
    wd = Path(working_directory)

    # Build a sid_selection string that create_log_file understands.
    # It accepts comma-separated individual IDs.
    sid_selection = ','.join(str(s) for s in scan_ids) if scan_ids else None

    log_file = str(wd / 'tomo_info.csv')
    create_log_file(
        log_file_name=log_file,
        working_directory=working_directory,
        sid_selection=sid_selection,
        skip_invalid=True,
    )

    process_proj(
        wd=working_directory,
        fn_param=parameters_file_name,
        fn_log=log_file,
        ic_name=ic_name,
        skip_processed=skip_processed,
    )

    Path(output_directory).mkdir(parents=True, exist_ok=True)

    # Scans occasionally come back with slightly different pixel counts
    # (the beamline trims rows/columns per scan). xrf_tomo's assembly
    # sizes everything from the first scan and fails on any mismatch, so
    # fall back to our padded assembly when the shapes disagree.
    filenames, _ = _used_files_sorted_by_theta(log_file, working_directory)
    shapes = {_fit_shape(Path(working_directory) / name)
              for name in filenames}
    if len(shapes) > 1:
        print(f'Projections have differing shapes ({sorted(shapes)}); '
              'assembling tomo.h5 with padding', flush=True)
        _make_single_hdf_padded(
            fn=Path(output_directory) / 'tomo.h5',
            fn_log=log_file,
            wd_src=Path(working_directory),
            ic_name=ic_name,
        )
    else:
        make_single_hdf(
            fn='tomo.h5',
            fn_log=log_file,
            wd_src=working_directory,
            wd_dest=output_directory,
            ic_name=ic_name,
        )

    if csv_output:
        csv_out = Path(csv_output)
        csv_out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(log_file, csv_out)
        print(f'CSV log copied to {csv_out}', flush=True)

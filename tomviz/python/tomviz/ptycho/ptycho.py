import math
from pathlib import Path
import sys

import numpy as np
from scipy.ndimage.interpolation import rotate
from scipy.optimize import leastsq
from tqdm import tqdm

from tomviz.executor import _write_emd
from tomviz.external_dataset import Dataset

PathLike = Path | str


def gather_ptycho_info(ptycho_dir: PathLike) -> dict:
    ptycho_dir = Path(ptycho_dir)

    sid_list = sorted([int(x.name[1:]) for x in ptycho_dir.iterdir()
                      if x.is_dir() and x.name.startswith('S')])

    sid_dirs = [ptycho_dir / f'S{sid}' for sid in sid_list]

    version_dict = {}
    for sid, d in zip(sid_list, sid_dirs):
        versions = []
        for subdir in d.iterdir():
            if subdir.is_dir():
                versions.append(subdir.name)

        if not versions:
            # There will be an error, but at least put in 't1'
            versions.append('t1')

        version_dict[sid] = sorted(versions)

    # Create angles and error lists for each SID and version
    angle_dict = {}
    error_dict = {}
    for sid in sid_list:
        these_angles = []
        these_errors = []
        for version in version_dict[sid]:
            these_angles.append(load_angle_from_sid(sid, version,
                                                    ptycho_dir))
            these_errors.append(validate_sid(sid, version, ptycho_dir))

        angle_dict[sid] = these_angles
        error_dict[sid] = these_errors

    version_list = [version_dict[sid] for sid in sid_list]
    angle_list = [angle_dict[sid] for sid in sid_list]
    error_list = [error_dict[sid] for sid in sid_list]
    return {
        'sid_list': sid_list,
        'version_list': version_list,
        'angle_list': angle_list,
        'error_list': error_list,
    }


def validate_sid(sid: int, version: str, ptycho_dir: PathLike) -> str:
    # Validate the sid and version, that it contains the data and angles.
    # If it is valid, the returned string will be empty. Otherwise, the
    # returned string will contain the error message as to what is not
    # valid.
    f_path = find_ptycho_file(sid, version, 'object', ptycho_dir)
    if f_path is None:
        return 'Ptycho data missing'

    g_path = find_ptycho_file(sid, version, 'probe', ptycho_dir)
    if g_path is None:
        return 'Probe data missing'

    angle = load_angle_from_sid(sid, version, ptycho_dir)
    if np.isnan(angle):
        return 'Angle not found'

    return ''


def find_ptycho_file(sid: int, version: str, type_str: str,
                     ptycho_dir: PathLike) -> Path | None:
    # type_str is `ptycho` or `probe`
    ptycho_dir = Path(ptycho_dir)
    dir_path = ptycho_dir / f'S{sid}/{version}/recon_data'
    base_str = f'recon_{sid}_{version}_{type_str}'
    # Prefer `_ave.npy` if available, then `.npy`
    suffix_to_try = [
        '_ave.npy',
        '.npy',
    ]
    for suffix in suffix_to_try:
        path = dir_path / f'{base_str}{suffix}'
        if path.exists():
            return path

    # If those didn't exist, just try to grab anything that
    # matches `{base_str}*.npy`
    paths = list(dir_path.glob(f'{base_str}*.npy'))
    if paths:
        return paths[0].resolve()

    # Didn't find any matches
    return None


def load_angle_from_sid(sid: int, version: str,
                        ptycho_dir: PathLike) -> float:
    path = locate_ptycho_hyan_file(sid, version, ptycho_dir)
    if path is None:
        return math.nan

    return fetch_angle_from_ptycho_hyan_file(path)


def locate_ptycho_hyan_file(sid: int, version: str,
                            ptycho_dir: PathLike) -> str | None:
    recon_data_dir = Path(ptycho_dir) / f'S{sid}' / version / 'recon_data'
    matches = list(recon_data_dir.glob(f'{sid}_{version}*'))

    for match in matches:
        try:
            with open(match.resolve(), 'r') as rf:
                if 'angle =' in rf.read():
                    return match.resolve()
        except Exception:
            # Move on to the next one
            continue

    return None


def get_use_and_versions_from_csv(csv_path: str) -> dict:
    data = np.genfromtxt(csv_path, delimiter=',', names=True, dtype=None)

    sids = []
    try:
        sids = data['Scan_ID'].tolist()
    except Exception:
        print('"Scan_ID" column not found in CSV file', file=sys.stderr)

    use = []
    try:
        use = data['Use'].tolist()
        # Convert to boolean
        use = [x in (1, '1', 'x') for x in use]
    except Exception:
        print('"Use" column not found in CSV file', file=sys.stderr)

    versions = []
    try:
        versions = data['Version'].tolist()
    except Exception:
        print('"Version" column not found in CSV file', file=sys.stderr)

    return {
        'sids': sids,
        'use': use,
        'versions': versions,
    }


def filter_sid_list(sid_list: list[int], filter_string: str) -> list[int]:
    if not filter_string.strip():
        # All SIDs are valid
        return list(sid_list)

    # Either a comma-delimited list or numpy slicing
    sid_strings = filter_string.split(',')
    valid_sids = []
    for this_str in sid_strings:
        if ':' in this_str:
            this_slice = slice(
                *(int(s) if s else None for s in this_str.split(':'))
            )
            # If there was no stop specified, go to the end of the sid_list
            if this_slice.stop is None:
                this_slice = slice(this_slice.start, max(sid_list) + 1,
                                   this_slice.step)

            valid_sids += np.r_[this_slice].tolist()
        else:
            valid_sids.append(int(this_str))

    return [sid for sid in sid_list if sid in valid_sids]


# Load all Ptycho data, stack with padded size for max
# Returns a path pointing to the output file that was created.
def load_stack_ptycho(version_list: list[str],
                      sid_list: list[int],
                      angle_list: list[float],
                      ptycho_dir: PathLike,
                      output_dir: PathLike,
                      rotate_datasets: bool = True) -> PathLike:

    filespty_obj = []
    filespty_prb = []
    currentsidlist = []

    if len(version_list) == 1:
        version_list = np.repeat(version_list, len(sid_list))

    ptycho_dir = Path(ptycho_dir)

    pixel_size_x = None
    pixel_size_y = None
    for i, sid in tqdm(enumerate(sid_list[0: len(version_list)]),
                       desc="Loading Ptycho"):
        sid = int(sid)
        version = version_list[i]
        f_path = find_ptycho_file(sid, version, 'object', ptycho_dir)
        g_path = find_ptycho_file(sid, version, 'probe', ptycho_dir)
        if f_path is not None:
            filespty_obj.append(f_path)
            currentsidlist.append((i, sid, angle_list[i], version))
            filespty_prb.append(g_path)
            if i == 0:
                print('Attempting to read pixel sizes from the '
                      f'first scan ID: {sid}')
                result = attempt_to_read_pixel_sizes(sid, version, ptycho_dir)
                if result is not None:
                    pixel_size_x, pixel_size_y = result
        else:
            print(f"didn't find: {sid}")

    has_pixel_sizes = pixel_size_x is not None
    print(f"found: {len(filespty_obj)}")

    tempPtyobj = []
    tempPtyprb = []
    tempPtyamp = []
    for i in tqdm(range(len(filespty_obj)), desc="processing ptycho"):
        obj = np.load(filespty_obj[i])
        prb = np.load(filespty_prb[i])

        if obj.ndim == 3:
            # Get the 0th order result
            obj = obj[0]

        if prb.ndim == 3:
            # Get the 0th order result
            prb = prb[0]

        space = 15
        obj = np.fliplr(np.rot90(obj))
        prb = np.fliplr(np.rot90(prb))
        prb_sz = np.shape(prb)
        # prb_sz_list.append(prb_sz)
        obj_sz = np.shape(obj)
        # obj_sz_list.append(obj_sz)
        obj_c = obj[
            int(prb_sz[0] / 2) + space: obj_sz[0] - int(prb_sz[0] / 2) - space,
            int(prb_sz[1] / 2) + space: obj_sz[1] - int(prb_sz[1] / 2) - space,
        ]
        # obj_c_sz_list.append(obj_c.shape)
        obj_c_arg = np.angle(obj_c)
        obj_c_amp = np.abs(obj_c)
        if True:
            obj_c_arg = remove_background(obj_c_arg)
        objectoutput = obj_c_amp * np.exp((0 + 1j) * obj_c_arg)
        obj_c_arg = np.angle(objectoutput)
        obj_c_amp = np.abs(obj_c)
        tempPtyamp.append(obj_c_amp)
        tempPtyobj.append(obj_c_arg * -1)
        tempPtyprb.append(prb)

    # imt = tf.imread(filelist[0][0])
    # load fluor1 and get shape of first array
    probes = np.asarray(tempPtyprb)
    probes_phase = np.angle(probes)
    probes_amp = np.abs(probes)
    # imt = ptfluor[0]
    # l, w = imt.shape
    # factor= 2
    shapeslist = [i.shape for i in tempPtyobj]
    shapeslist = np.asarray(shapeslist)
    lmax = shapeslist[:, 0].max()
    wmax = shapeslist[:, 1].max()
    ptychodatanew = np.zeros((len(tempPtyobj), int(lmax), int(wmax)))
    ampdatanew = np.zeros((len(tempPtyobj), int(lmax), int(wmax)))
    # Loop checking size, find largest, #try for if its larger, crop step
    for n, i in tqdm(enumerate(tempPtyobj), desc="correcting shape"):
        lerr = np.abs(i.shape[0] - lmax)
        werr = np.abs(i.shape[1] - wmax)
        ti = np.pad(i, ((lerr // 2, lerr // 2), (werr // 2, werr // 2)))
        ta = np.pad(
            tempPtyamp[n], ((lerr // 2, lerr // 2), (werr // 2, werr // 2)),
        )
        ptychodatanew[n, :, :] = ti
        ampdatanew[n, :, :] = ta

    arrays = {
        'Phase': ptychodatanew,
        'Amplitude': ampdatanew,
        'Probes Phase': probes_phase,
        'Probes Amplitude': probes_amp,
    }

    # Do all necessary processing of the output arrays.
    for key, array in arrays.items():
        if rotate_datasets:
            array = rotate(array, -90.0, axes=(1, 2))

        array = array.swapaxes(0, 2)
        arrays[key] = array

    # Make sure the output directory exists
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    # Now write out the datasets in EMD format
    output_files = []
    datasets = {
        # Ptycho and Amp have the same shape, so we write them together
        'ptycho_object.emd': ['Phase', 'Amplitude'],
        # Probe has a different shape
        'ptycho_probe.emd': ['Probes Phase', 'Probes Amplitude'],
    }
    for filename, array_names in datasets.items():
        dataset = Dataset({key: arrays[key] for key in array_names})
        dataset.tilt_angles = np.array([x[2] for x in currentsidlist])
        dataset.tilt_axis = 2
        if filename == 'ptycho_object.emd' and has_pixel_sizes:
            # Also set the pixel sizes if they are available
            dataset.spacing = (pixel_size_x, pixel_size_y, 1)
        output_path = Path(output_dir) / filename
        _write_emd(output_path, dataset)
        output_files.append(str(output_path))

    # Now write out the text file containing info about what was used
    # FIXME: also add reconstruction pixel sizes
    currentsidlist_str = []
    for row in currentsidlist:
        this_row = []
        for entry in row:
            if isinstance(entry, float):
                s = f'{entry:.3f}'
            else:
                s = entry
            this_row.append(s)

        currentsidlist_str.append(this_row)

    col_delim = ' '
    headers = ['Angle', 'SID', 'Version']
    index_order = [2, 1, 3]
    col_width = 10
    with open(Path(output_dir) / 'stacked_ptycho_info.txt', 'w') as wf:
        header_str = col_delim.join([f'{x:>{col_width}}' for x in headers])
        # Replace first character with '#'
        header_str = '#' + header_str[1:]
        wf.write(header_str + '\n')
        for row in currentsidlist_str:
            row_str = col_delim.join([
                f'{row[idx]:>{col_width}}' for idx in index_order
            ])
            wf.write(row_str + '\n')

    return output_files


def remove_background(im: np.ndarray) -> np.ndarray:
    params = [0, 0, 0]

    def err_func(p):
        return np.ravel(fit_func(*p)(*np.indices(im.shape)) - im)

    p, success = leastsq(err_func, params)
    return im - fit_func(*p)(*np.indices(im.shape))


def fit_func(
    p0,
    px1,
    py1,
):
    return lambda x, y: p0 + x * px1 + py1 * y


def attempt_to_read_pixel_sizes(sid, version, ptycho_dir) -> tuple[float, float] | None:
    path = locate_ptycho_hyan_file(sid, version, ptycho_dir)
    if path is None:
        print(
            f'Failed to locate config file for {sid} {version}\n'
            f'Pixel sizes will not be read',
            file=sys.stderr,
        )
        return None


    result = fetch_pixel_sizes_from_ptycho_hyan_file(path)
    if result is None:
        print(
            'Failed to obtain pixel sizes. Pixel sizes '
            'will not be applied to datasets.',
            file=sys.stderr,
        )
        return None

    print('Pixel sizes identified as:', result[0], result[1])
    return result[0], result[1]


def fetch_angle_from_ptycho_hyan_file(filepath: PathLike) -> float | None:
    with open(filepath, 'r') as rf:
        for line in rf:
            line = line.lstrip()
            if line.startswith('angle = '):
                angle = float(line.split('=')[1].strip())
                return angle

    # Angle was not found
    return math.nan


def fetch_pixel_sizes_from_ptycho_hyan_file(
    filepath: PathLike,
) -> tuple[float, float] | None:
    print(f'Obtaining pixel sizes from config file: {filepath})')
    vars_required = [
        'lambda_nm', 'z_m', 'x_arr_size', 'y_arr_size', 'ccd_pixel_um'
    ]
    results = {}
    try:
        with open(filepath, 'r') as rf:
            for line in rf:
                if '=' not in line:
                    continue

                lhs = line.split('=')[0].strip()
                if lhs in vars_required:
                    value = float(line.split('=')[1].strip())
                    results[lhs] = value
    except Exception as e:
        print('Failed to fetch pixel sizes with error:', e, file=sys.stderr)
        return None

    missing = [x for x in vars_required if x not in results]
    if missing:
        print(
            'Failed to fetch pixel sizes. Some required variables '
            f'were not found: {missing}'
        )
        return None

    # Now compute them. They can both use the same numerator
    numerator = (
        results['lambda_nm'] * results['z_m'] * 1e6 / results['ccd_pixel_um']
    )

    x_pixel_size = numerator / results['x_arr_size']
    y_pixel_size = numerator / results['y_arr_size']

    return x_pixel_size, y_pixel_size

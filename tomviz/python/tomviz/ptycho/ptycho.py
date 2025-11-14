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

    sid_list = [int(x.name[1:]) for x in ptycho_dir.iterdir()
                if x.is_dir() and x.name.startswith('S')]

    sid_dirs = [ptycho_dir / f'S{sid}' for sid in sid_list]

    version_list = set()
    for d in sid_dirs:
        for subdir in d.iterdir():
            if subdir.is_dir():
                version_list.add(subdir.name)

    version_list = list(sorted(version_list))

    # Create angles for each SID and version
    angles_dict = {}
    for sid in sid_list:
        angles_dict[sid] = {}
        sid_dir = ptycho_dir / f'S{sid}'
        for version_dir in sid_dir.iterdir():
            angle = load_angle_from_sid(sid, version_dir.name, ptycho_dir)
            if angle is not None:
                angles_dict[sid][version_dir.name] = angle

    return {
        'sid_list': sid_list,
        'version_list': version_list,
        'angles_dict': angles_dict,
        # We return these so it's easier to identify them in C++
        'min_sid': min(sid_list) if sid_list else 0,
        'max_sid': max(sid_list) if sid_list else 0,
    }


def find_missing_data(version_list: list[str],
                      sid_list: list[int],
                      ptycho_dir: PathLike) -> list[tuple[str, int]]:
    missing = []
    ptycho_dir = Path(ptycho_dir)
    for version, sid in zip(version_list, sid_list):
        dir_path = ptycho_dir / f'S{sid}/{version}/recon_data'
        f_path = dir_path / f'recon_{sid}_{version}_object_ave.npy'
        g_path = dir_path / f'recon_{sid}_{version}_probe_ave.npy'
        if not f_path.exists() or not g_path.exists():
            missing.append((version, sid))

    return missing


def find_sids_missing_angles(sid_list: list[int],
                             angle_list: list[float]) -> list[int]:
    return [sid_list[i] for i in range(len(angle_list))
            if np.isnan(angle_list[i])]


def remove_sids_missing_data_or_angles(version_list: list[str],
                                       sid_list: list[int],
                                       angle_list: list[float],
                                       ptycho_dir: PathLike) -> bool:
    missing_data = find_missing_data(version_list, sid_list, ptycho_dir)
    missing_angles = find_sids_missing_angles(sid_list, angle_list)

    for version, sid in missing_data:
        # Find it in the sid_list and angle_list and remove it
        for i in range(len(sid_list)):
            if sid_list[i] == sid:
                angle = angle_list[i]
                print(f'Removing SID missing data: {sid} (angle: {angle})')
                sid_list.pop(i)
                angle_list.pop(i)
                version_list.pop(i)
                break

    for sid in missing_angles:
        for i in range(len(sid_list)):
            if sid_list[i] == sid:
                print('Removing SID missing angle:', sid)
                sid_list.pop(i)
                angle_list.pop(i)
                version_list.pop(i)
                break

    if not sid_list or not angle_list or not version_list:
        raise RuntimeError('All SIDs were invalid')

    return True


def load_angle_from_sid(sid: int, version: str,
                        ptycho_dir: PathLike) -> float:
    recon_data_dir = Path(ptycho_dir) / f'S{sid}' / version / 'recon_data'
    matches = list(recon_data_dir.glob('*ptycho_hyan*.txt'))
    if not matches:
        return np.nan

    return fetch_angle_from_ptycho_hyan_file(matches[0].resolve())


def load_sids_from_file(filepath: PathLike) -> list:
    # FIXME: we need a real example. This might not be right.
    path = Path(filepath)
    if path.suffix == '.txt':
        delimiter = None
    elif path.suffix == '.csv':
        delimiter = ','
    else:
        raise RuntimeError(f'Unhandled extension: {filepath}')

    sids, angles = np.loadtxt(
        filepath,
        usecols=(0, 1),
        delimiter=delimiter,
    ).T.tolist()
    return sids, angles


def create_sid_list(load_sid_settings: dict, ptycho_dir: PathLike) -> dict:
    output_version_list = []
    sid_list = []
    angle_list = []

    version_list = load_sid_settings['version_list']
    load_methods = load_sid_settings['load_methods']
    load_sids = load_sid_settings['load_sids']

    for version, method, sid_str in zip(version_list, load_methods, load_sids):
        if not sid_str.strip():
            # No file, nor numpy splicing, was selected.
            continue

        angles = None
        if method == 'from_file':
            # Load the SID list from a file
            new_sids, angles = load_sids_from_file(sid_str)
        else:
            # Either a comma-delimited list or numpy slicing
            sid_strings = sid_str.split(',')
            new_sids = []
            for this_str in sid_strings:
                if ':' in this_str:
                    this_slice = slice(
                        *(int(s) if s else None for s in this_str.split(':'))
                    )
                    new_sids += np.r_[this_slice].tolist()
                else:
                    new_sids.append(int(this_str))

        if angles is None:
            # Find all angles for the new SIDs
            # Some of these angles might be None, if they could not
            # be found. We'll have to check on that later.
            angles = [load_angle_from_sid(sid, version, ptycho_dir)
                      for sid in new_sids]

        angle_list += angles
        sid_list += new_sids
        output_version_list += ([version] * len(angles))

    def sort_with_nan(x):
        if np.isnan(x[0]):
            return 1e300

        return x[0]

    # Now sort them by angle
    sorted_version_list = []
    sorted_sid_list = []
    sorted_angle_list = []
    for angle, sid, version in sorted(
        zip(angle_list, sid_list, output_version_list),
        key=sort_with_nan,
    ):
        sorted_version_list.append(version)
        sorted_sid_list.append(sid)
        sorted_angle_list.append(angle)

    return {
        'version_list': sorted_version_list,
        'sid_list': sorted_sid_list,
        'angle_list': sorted_angle_list,
    }


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
        dir_path = ptycho_dir / f'S{sid}/{version}/recon_data'
        f_path = dir_path / f'recon_{sid}_{version}_object_ave.npy'
        g_path = dir_path / f'recon_{sid}_{version}_probe_ave.npy'
        if f_path.exists():
            filespty_obj.append(f_path)
            currentsidlist.append((i, sid, angle_list[i], version))
            filespty_prb.append(g_path)
            if i == 0:
                print('Attempting to read pixel sizes from the '
                      f'first scan ID: {sid}')
                result = attempt_to_read_pixel_sizes(dir_path)
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


def attempt_to_read_pixel_sizes(dir_path: Path) -> tuple[float, float] | None:
    matches = list(dir_path.glob('*ptycho_hyan*.txt'))
    if not matches:
        print(
            f'Failed to locate config file in {dir_path}\n'
            f'Pixel sizes will not be read',
            file=sys.stderr,
        )
        return None

    result = fetch_pixel_sizes_from_ptycho_hyan_file(matches[0].resolve())
    if result is None:
        print(
            f'Failed to obtain pixel sizes. Pixel sizes '
            f'will not be applied to datasets.',
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
    return np.nan


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

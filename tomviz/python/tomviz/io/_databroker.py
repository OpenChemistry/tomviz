import os
import numpy as np
import tomviz.internal_utils

from vtk import vtkImageData

_installed = False
DEFAULT_URL = "https://tiled.nsls2.bnl.gov/api"
TILED_URL = os.getenv("TILED_URL", DEFAULT_URL)
try:
    from tiled.client import from_uri
    from tiled.client.cache import Cache
    #from tiled.client import from_profile
    #c = from_profile("fxi")
    from databroker.queries import TimeRange, ScanID
    _installed = True
    c = None
except ImportError:
    pass
except Exception:
    import traceback
    traceback.print_exc()


def installed():
    return _installed


def initialize():
    global c
    if c is None:
        c = from_uri(TILED_URL, cache=Cache(capacity=1e6))


def catalogs():
    initialize()

    cats = []
    for name in c:
        cats.append({
            "name": name,
            "description": "blank" #cat.describe()['description']
        })

    cats = sorted(cats, key=lambda c: c['name'])

    return cats


def runs(catalog_name, id, since, until, limit):
    initialize()

    runs = []

    current = c[catalog_name]['raw']

    if since != "" and until != "":
        current = current.search(TimeRange(since=since, until=until))

    if id != -1:
        current = current.search(ScanID(id))

    cat = current.values_indexer[-1:-limit:-1]

    for run in cat:
        md = run.metadata
        r = {
            "uid": md['start']['uid'],
            "planName": md['start']['plan_name'],
            "scanId": md['start']['scan_id'],
            "startTime": md['start']['time'],
        }

        if md.get('stop') is not None:
            r['stopTime'] = md['stop']['time']

        runs.append(r)

    runs = sorted(runs, key=lambda r: r['startTime'])

    return runs


def tables(catalog_name, run_uid):
    tables = []
    for name, _ in c[catalog_name]['raw'][run_uid].items():
        tables.append({
            "name": name,
        })

    tables = sorted(tables, key=lambda t: t['name'])

    return tables


def variables(catalog_name, run_uid, table):
    variables = []

    # Would be nice to use context manager here, but it doesn't seem to close
    # the dataset.
    dataset = c[catalog_name]['raw'][run_uid][table].read()
    for name, variable in dataset.data_vars.items():
        variables.append({
            "name": name,
            "size": variable.data.nbytes
        })

    variables = sorted(variables, key=lambda v: v['name'])

    return variables


def _nsls2_fxi_load_thetas(run):
    """Load thetas from fly scan. The stage has a monitor and the timestamps
       from the camera have to be interpolated with the stage to get the stage
       position.
    """
    EPICS_TO_UNIX_OFFSET = 631152000  # 20 years in seconds
    # get xarray object once, then grab angles and time from it
    zps_pi_r = run['zps_pi_r_monitor']['data']['zps_pi_r'][:]
    rotation_deg = np.array(zps_pi_r)
    # UNIX EPOCH
    #rotation_time = np.array(zps_pi_r['time'][:])
    rotation_time = run['zps_pi_r_monitor']['data']['time'][:]
    # EPICS EPOCH
    img_time = np.array(run['primary']['data']['Andor_timestamps'][:])
    img_time = np.concatenate(img_time, axis=0)  # remove chunking
    img_time += EPICS_TO_UNIX_OFFSET  # Convert EPICS to UNIX TIMESTAMP
    # Interpolate rotation angle for each projection
    #thetas = np.deg2rad(np.interp(img_time, rotation_time, rotation_deg))
    thetas = np.interp(img_time, rotation_time, rotation_deg)
    return thetas


def load_variable(catalog_name, run_uid, table, variable):
    if run_uid not in c[catalog_name]['raw']:
        raise Exception(f"Unable to load run: {run_uid}")

    if table not in c[catalog_name]['raw'][run_uid]:
        raise Exception(f"Unable to find table: {table}")

    if variable not in c[catalog_name]['raw'][run_uid][table]['data']:
        raise Exception(f"Unable to find variable: {variable}")

    run = c[catalog_name]['raw'][run_uid]
    data = run[table]['data'][variable].read()
    shape = data.shape
    data = data.reshape((shape[0]*shape[1], shape[2], shape[3]))
    # Convert to numpy array
    data = np.asfortranarray(data)

    image_data = vtkImageData()
    (x, y, z) = data.shape

    # Metadata?
    image_data.SetOrigin(0, 0, 0)
    image_data.SetSpacing(1, 1, 1)
    image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
    tomviz.internal_utils.set_array(image_data, data)

    # Now load the angles
    if 'zps_pi_r_monitor' not in run or \
       'data' not in run['zps_pi_r_monitor'] or \
       'zps_pi_r' not in run['zps_pi_r_monitor']['data']:
        raise Exception("No angles found!")

    angles = _nsls2_fxi_load_thetas(run)
    tomviz.internal_utils.set_tilt_angles(image_data, angles)

    return image_data


def save_data(catalog_name, name, data):
    scalars = tomviz.internal_utils.get_scalars(data)
    r = c[catalog_name]["sandbox"].write_array(scalars, metadata={"name": name})

    return r

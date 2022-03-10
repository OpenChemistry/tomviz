import os
import numpy as np

import tomviz.utils

from vtk import vtkImageData

_installed = False
DEFAULT_URL = "https://tiled.nsls2.bnl.gov"
TILED_URL = os.getenv("TILED_URL", DEFAULT_URL)
try:
    from tiled.client import from_uri
    c = from_uri(TILED_URL, "dask")
    from databroker.queries import TimeRange
    _installed = True
except ImportError:
    pass


def installed():
    return _installed


def catalogs():
    cats = []
    for name in c:
        cats.append({
            "name": name,
            "description": "blank" #cat.describe()['description']
        })

    cats = sorted(cats, key=lambda c: c['name'])

    return cats


def runs(catalog_name, since, until, limit):
    runs = []

    current = c[catalog_name]

    if since != "" and until != "":
        current = current.search(TimeRange(since=since, until=until))

    cat = current.values_indexer[-1:-limit:-1]

    for run in cat:
        md = run.metadata
        runs.append({
            "uid": md['start']['uid'],
            "planName": md['start']['plan_name'],
            "scanId": md['start']['scan_id'],
            "startTime": md['start']['time'],
        })

    runs = sorted(runs, key=lambda r: r['uid'])

    return runs


def tables(catalog_name, run_uid):
    tables = []
    for name, _ in c[catalog_name][run_uid].items():
        tables.append({
            "name": name,
        })

    tables = sorted(tables, key=lambda t: t['name'])

    return tables


def variables(catalog_name, run_uid, table):
    variables = []

    # Would be nice to use context manager here, but it doesn't seem to close
    # the dataset.
    dataset = c[catalog_name][run_uid][table].read()
    for name, variable in dataset.data_vars.items():
        variables.append({
            "name": name,
            "size": variable.data.nbytes
        })

    variables = sorted(variables, key=lambda v: v['name'])

    return variables


def load_variable(catalog_name, run_uid, table, variable):
    data = c[catalog_name][run_uid][table]['data'][variable].data
    shape = data.shape
    data = data.reshape((shape[0]*shape[1], shape[2], shape[3]))

    # Convert to Fortran ordering
    data = np.asfortranarray(data)

    image_data = vtkImageData()
    (x, y, z) = data.shape

    # Metadata?
    image_data.SetOrigin(0, 0, 0)
    image_data.SetSpacing(1, 1, 1)
    image_data.SetExtent(0, x - 1, 0, y - 1, 0, z - 1)
    tomviz.utils.set_array(image_data, data)

    return image_data

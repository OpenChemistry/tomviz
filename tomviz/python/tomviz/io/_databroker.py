import numpy as np

import tomviz.utils

from vtk import vtkImageData

_installed = False
try:
    from databroker import catalog
    from databroker.queries import TimeRange
    _installed = True
except ImportError:
    pass


def installed():
    return _installed


def catalogs():
    cats = []
    for name, cat in catalog.items():
        cats.append({
            "name": name,
            "description": cat.describe()['description']
        })

    cats = sorted(cats, key=lambda c: c['name'])

    return cats


def runs(catalog_name, since, until):
    runs = []

    cat = catalog[catalog_name]

    if since != "" and until != "":
        cat = cat.search(TimeRange(since=since, until=until))

    for uid, run in cat.items():
        runs.append({
            "uid": uid,
            "planName": run.metadata['start']['plan_name'],
            "scanId": run.metadata['start']['scan_id'],
            "startTime": run.metadata['start']['time'],
            "stopTime": run.metadata['stop']['time'],
        })

    runs = sorted(runs, key=lambda r: r['uid'])

    return runs


def tables(catalog_name, run_uid):
    tables = []
    for name, _ in catalog[catalog_name][run_uid].items():
        tables.append({
            "name": name,
        })

    tables = sorted(tables, key=lambda t: t['name'])

    return tables


def variables(catalog_name, run_uid, table):
    variables = []

    # Would be nice to use context manager here, but it doesn't seem to close
    # the dataset.
    dataset = catalog[catalog_name][run_uid][table].read()
    try:
        for name, variable in dataset.data_vars.items():
            variables.append({
                "name": name,
                "size": variable.data.nbytes
            })
    finally:
        dataset.close()

    variables = sorted(variables, key=lambda v: v['name'])

    return variables


def load_variable(catalog_name, run_uid, table, variable):
    dataset = catalog[catalog_name][run_uid][table].read()
    try:
        data = dataset[variable].data
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
    finally:
        dataset.close()

    return image_data

# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
from typing import Callable
import fnmatch
import inspect
import json
import os
import traceback


# Generic operator-runtime helpers now live in the standalone
# tomviz-pipeline library; re-export them so C++ callers and operator
# scripts keep finding them under tomviz._internal. find_operator_class
# recognizes subclasses of both tomviz.operators.Operator and
# tomviz_pipeline.operators.Operator (see tomviz_pipeline._compat).
from tomviz_pipeline._internal import (  # noqa: F401
    OperatorWrapper,
    _find_function,
    _operator_method_was_implemented,
    apply_decorator,
    find_operator_class,
    find_transform_from_module,
    find_transform_function,
    has_decorator,
)


def in_application():
    return os.environ.get('TOMVIZ_APPLICATION', False)


def require_internal_mode():
    if not in_application():
        func_name = str(inspect.currentframe().f_back.f_code.co_name)
        raise Exception('Cannot call ' + func_name + ' in external mode')


def add_transform_decorators(transform_method: Callable,
                             operator_dict: dict) -> Callable:
    """Optionally add any transform wrappers that we need to add

    Currently, this adds `@apply_to_each_array` automatically if
    `"apply_to_each_array": false` is not set within the json
    description, and if the decorator was not already applied.
    """
    add_apply_to_each_array = True
    operator_description = operator_dict.get('description')
    if operator_description:
        description_json = json.loads(operator_description)
        if not description_json.get('apply_to_each_array', True):
            # It was intentionally disabled in the json
            add_apply_to_each_array = False

    if transform_method.__name__ == 'transform_scalars':
        # This is an old transform function. We don't want to do any
        # kind of automatic modifications to the old ones.
        add_apply_to_each_array = False

    if add_apply_to_each_array:
        # First, make sure it wasn't already decorated
        if not has_decorator(transform_method, 'apply_to_each_array'):
            # Decorate it!
            from tomviz.utils import apply_to_each_array
            transform_method = apply_decorator(transform_method,
                                               apply_to_each_array)

    return transform_method


def transform_method_wrapper(transform_method: Callable,
                             operator_serialized: str, *args, **kwargs):
    # External execution is now driven from C++ via the per-node
    # ExternalNodeExecutor strategy (see tomviz/pipeline/
    # ExternalNodeExecutor.h). The wrapper still receives the serialized
    # operator JSON for symmetry with the legacy entry point and so any
    # future Python-side dispatch decisions have it on hand, but the
    # `tomviz_pipeline_env` branch was removed: when it's set on the
    # operator description, LegacyPythonTransform synthesizes a
    # NodeExecutor at deserialize time and the in-app run happens out of
    # process before Python is even involved.
    operator_dict = json.loads(operator_serialized)
    transform_method = add_transform_decorators(transform_method, operator_dict)
    return transform_method(*args, **kwargs)


def _classify_from_json(operator_json):
    # Schema v2 with non-empty inputs is a transform; schema v2 without inputs
    # is a source. Anything else (no schemaVersion, or v1) is a legacy
    # transform.
    if operator_json.get('schemaVersion') == 2:
        if operator_json.get('inputs'):
            return 'transform'
        return 'source'
    return 'legacy_transform'


def _operator_description(operator_dir, filename):
    name, _ = os.path.splitext(filename)
    description = {
        'label': name,
        'pythonPath': os.path.join(operator_dir, filename),
        'valid': True,
        'type': 'legacy_transform',
    }

    json_filepath = os.path.join(operator_dir, '%s.json' % name)
    if os.path.exists(json_filepath):
        description['jsonPath'] = json_filepath
        try:
            with open(json_filepath, encoding='utf-8') as fp:
                operator_json = json.load(fp)
            description['label'] = operator_json.get('label', name)
            description['type'] = _classify_from_json(operator_json)
        except Exception:
            description['loadError'] = traceback.format_exc()
            description['valid'] = False

    return description


def find_operators(operator_dir):
    # First look for the python files
    python_files = fnmatch.filter(os.listdir(operator_dir), '*.py')
    operator_descriptions = []
    for python_file in python_files:
        operator_descriptions.append(
            _operator_description(operator_dir, python_file)
        )

    return operator_descriptions


def convert_to_dataset(data):
    # This method will extract/convert certain data types to a dataset

    from tomviz.dataset import Dataset

    if isinstance(data, Dataset):
        # It is already a dataset
        return data

    if hasattr(data, 'IsA'):
        # A bare VTK data object: wrap it in a numpy-backed
        # LegacyDataset whose arrays are views over the VTK buffers.
        from tomviz._boundary import wrap_vtk_image
        return wrap_vtk_image(data, legacy=True)

    msg = 'Cannot convert type to Dataset: ' + str(type(data))
    raise Exception(msg)


def convert_to_vtk_data_object(data):
    # This method will extract/convert certain data types to a
    # vtkDataObject (VTK objects pass through untouched).

    from tomviz._boundary import payload_to_vtk

    converted = payload_to_vtk(data)
    if converted is None:
        msg = 'Cannot convert type to vtkDataObject: ' + str(type(data))
        raise Exception(msg)
    return converted


def with_vtk_dataobject(f):
    # A decorator to automatically convert the first argument to a
    # vtkDataObject; when it was a numpy-backed dataset, its views are
    # refreshed afterwards so mutations made through VTK are visible.
    # This also confirms we are running internally.

    def wrapped(*args, **kwargs):
        if not in_application():
            name = f.__name__
            raise Exception('Cannot call ' + name + ' in external mode')

        data = args[0]
        dataobject = convert_to_vtk_data_object(data)
        args = (dataobject, *args[1:])
        result = f(*args, **kwargs)
        if not hasattr(data, 'IsA'):
            from tomviz._boundary import refresh_dataset
            refresh_dataset(data)
        return result

    return wrapped


def with_dataset(f):
    # A decorator to automatically convert the first argument to a
    # Dataset; when it was a bare VTK object, changes are flushed back
    # into it afterwards.

    def wrapped(*args, **kwargs):
        data = args[0]
        dataset = convert_to_dataset(data)
        args = (dataset, *args[1:])
        result = f(*args, **kwargs)
        if dataset is not data:
            from tomviz._boundary import flush_dataset
            flush_dataset(dataset)
        return result

    return wrapped

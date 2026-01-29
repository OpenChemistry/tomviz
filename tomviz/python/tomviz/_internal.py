# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
import fnmatch
import importlib.machinery
import importlib.util
import inspect
import json
import os
import subprocess
import sys
import tempfile
import traceback

from pathlib import Path
from typing import Callable

import tomviz
import tomviz.operators


def in_application():
    return os.environ.get('TOMVIZ_APPLICATION', False)


if in_application():
    import tomviz._wrapping
    from vtk import vtkDataObject


def require_internal_mode():
    if not in_application():
        func_name = str(inspect.currentframe().f_back.f_code.co_name)
        raise Exception('Cannot call ' + func_name + ' in external mode')


def delete_module(name):
    if name in sys.modules:
        del sys.modules[name]


def find_operator_class(transform_module):
    operator_class = None
    classes = inspect.getmembers(transform_module, inspect.isclass)
    for (name, cls) in classes:
        if issubclass(cls, tomviz.operators.Operator):
            if operator_class is not None:
                raise Exception('Multiple operators define in module, only '
                                'one operator can be defined per module.')

            operator_class = cls

    return operator_class


def _find_function(module, function_name):
    # Finds a function in the module with a given "function_name"
    # Returns `None` if it is not found
    functions = inspect.getmembers(module, inspect.isfunction)
    for (name, func) in functions:
        if name == function_name:
            return func


def find_transform_from_module(transform_module):
    # This tries to first find transform(), and then transform_scalars()
    f = _find_function(transform_module, 'transform')
    if f is None:
        f = _find_function(transform_module, 'transform_scalars')

    return f


def is_cancelable(transform_module):
    cls = find_operator_class(transform_module)

    if cls is None:
        function = find_transform_from_module(transform_module)

    if cls is None and function is None:
        raise Exception('Unable to locate function or operator class.')

    return cls is not None and issubclass(cls,
                                          tomviz.operators.CancelableOperator)


def is_completable(transform_module):
    cls = find_operator_class(transform_module)

    if cls is None:
        function = find_transform_from_module(transform_module)
        if not function:
            raise Exception('Unable to locate function or operator class.')

    return cls is not None and issubclass(
        cls,
        tomviz.operators.CompletableOperator
    )


def find_transform_function(transform_module, op=None):

    transform_function = find_transform_from_module(transform_module)
    if transform_function is None:
        cls = find_operator_class(transform_module)
        if cls is None:
            raise Exception('Unable to locate transform function.')

        # We call __new__ and __init__ manually here so we can inject the
        # wrapper OperatorPython instance before __init__ is called so that
        # any code in __init__ can access the wrapper.
        o = cls.__new__(cls)
        if op is not None:
            # Set the wrapped OperatorPython instance
            o._operator_wrapper = tomviz._wrapping.OperatorPythonWrapper(op)
        cls.__init__(o)

        transform_function = None
        if _operator_method_was_implemented(o, 'transform'):
            transform_function = o.transform
        elif _operator_method_was_implemented(o, 'transform_scalars'):
            transform_function = o.transform_scalars

    if transform_function is None:
        raise Exception('Unable to locate transform function.')

    return transform_function


def transform_method_wrapper(transform_method: Callable,
                             operator_serialized: str, *args, **kwargs):
    # We take the serialized operator as input because we may need it
    # later. If we need to execute this in an external environment,
    # we need it serialized. It's easier to have the C++ side do the
    # serialization right now.
    operator_dict = json.loads(operator_serialized)
    tomviz_pipeline_env = None

    operator_description = operator_dict.get('description')
    if operator_description:
        description_json = json.loads(operator_description)
        tomviz_pipeline_env = description_json.get('tomviz_pipeline_env')

    if not tomviz_pipeline_env:
        # Execute internally as normal
        return transform_method(*args, **kwargs)

    return transform_single_external_operator(transform_method, operator_serialized, *args, **kwargs)


def transform_single_external_operator(transform_method: Callable,
                                       operator_serialized: str, *args, **kwargs):
    from tomviz.executor import load_dataset, _write_emd

    operator_dict = json.loads(operator_serialized)
    description_dict = json.loads(operator_dict['description'])
    tomviz_pipeline_env = description_dict['tomviz_pipeline_env']

    # Find the `tomviz-pipeline` executable
    exec_path = Path(tomviz_pipeline_env) / 'bin/tomviz-pipeline'
    if not exec_path.exists():
        msg = f'Tomviz pipeline executable does not exist: {exec_path}'
        raise RuntimeError(msg)

    # Get the dataset as a Dataset class
    dataset = convert_to_dataset(args[0])

    # Set up the temporary directory for reading/writing
    with tempfile.TemporaryDirectory() as tmpdir_path:
        tmpdir_path = Path(tmpdir_path)

        # Save the input data to an EMD file
        input_path = tmpdir_path / 'original.emd'
        _write_emd(input_path, dataset)

        # Build the state file dict
        state_dict = {
            'dataSources': [{
                'reader': {
                    'fileNames': [str(input_path)],
                },
                'operators': [operator_dict],
            }],
        }

        # Write the state file
        state_path = tmpdir_path / 'state.tvsm'
        with open(state_path, 'w') as wf:
            json.dump(state_dict, wf)

        output_path = tmpdir_path / 'output.emd'

        progress_path = tmpdir_path / 'progress'
        # FIXME: set up 'socket' progress mode for Linux, and
        # 'files' progress mode for Mac/Windows. We need to implement
        # a Python reader for each of these. They should just forward
        # progress to the main progress.
        progress_mode = 'tqdm'

        # Set up the command
        cmd = [
            exec_path,
            '-s',
            state_path,
            '-o',
            output_path,
            '-p',
            progress_mode,
            '-u',
            progress_path,
        ]
        cmd = [str(x) for x in cmd]

        # Slightly customize the environment
        custom_env = os.environ.copy()
        custom_env.pop('TOMVIZ_APPLICATION', None)
        custom_env.pop('PYTHONHOME', None)
        custom_env.pop('PYTHONPATH', None)
        custom_env['PYTHONUNBUFFERED'] = 'ON'

        print('Executing operator with command:', ' '.join(cmd))

        # Run the operator
        subprocess.run(cmd, check=True, env=custom_env)

        # Load and return the result
        output_dataset = load_dataset(output_path)

    # Now modify the input dataset with any changes made.
    # FIXME: put these in a function to copy one dataset to another?
    for name in output_dataset.scalars_names:
        dataset.set_scalars(name, output_dataset.scalars(name))
    dataset.tilt_angles = output_dataset.tilt_angles
    dataset.spacing = output_dataset.spacing

    # FIXME: for functions that normally return a dict with things like
    # a child dataset, what should we do?
    return None


def _load_module(operator_dir, python_file):
    module_name, _ = os.path.splitext(python_file)
    spec = importlib.machinery.PathFinder.find_spec(module_name, [operator_dir])
    if spec is None:
        raise ImportError(f"No module named '{module_name}' found in {operator_dir}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    if spec.loader:
        spec.loader.exec_module(module)
    return module


def _has_operator(module):
    return find_transform_from_module(module) is not None or \
        find_operator_class(module) is not None


def _operator_description(operator_dir, filename):
    name, _ = os.path.splitext(filename)
    description = {
        'label': name,
        'pythonPath': os.path.join(operator_dir, filename),
    }

    has_operator = False
    # Load the module and see if they are valid operators
    try:
        module = _load_module(operator_dir, filename)
        has_operator = _has_operator(module)
    except Exception:
        description['loadError'] = traceback.format_exc()

    description['valid'] = has_operator

    # See if we have a JSON file
    json_filepath = os.path.join(operator_dir, '%s.json' % name)
    if os.path.exists(json_filepath):
        description['jsonPath'] = json_filepath
        # Extract the label from the JSON
        try:
            with open(json_filepath) as fp:
                operator_json = json.load(fp)
                description['label'] = operator_json['label']
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


def _operator_method_was_implemented(obj, method):
    # It would be nice if there were an easier way to do this, but
    # I am not currently aware of an easier way.
    bases = list(inspect.getmro(type(obj)))
    # We know operator has this attribute, remove it
    bases.remove(tomviz.operators.Operator)

    for base in bases:
        if method in vars(base):
            return True

    return False


def convert_to_dataset(data):
    # This method will extract/convert certain data types to a dataset

    if in_application():
        from tomviz.internal_dataset import Dataset
    else:
        from tomviz.external_dataset import Dataset

    if isinstance(data, Dataset):
        # It is already a dataset
        return data

    if in_application():
        if isinstance(data, vtkDataObject):
            # Make a new dataset object with no Datasource
            return Dataset(data, None)

    msg = 'Cannot convert type to Dataset: ' + str(type(data))
    raise Exception(msg)


def convert_to_vtk_data_object(data):
    # This method will extract/convert certain data types to a vtkDataObject

    from tomviz.internal_dataset import Dataset

    if isinstance(data, vtkDataObject):
        # It is already a vtkDataObject
        return data

    if isinstance(data, Dataset):
        # Should be stored in _data_object
        return data._data_object

    msg = 'Cannot convert type to vtkDataObject: ' + str(type(data))
    raise Exception(msg)


def with_vtk_dataobject(f):
    # A decorator to automatically convert the first argument to
    # a vtkDataObject. This also confirms we are running internally.

    def wrapped(*args, **kwargs):
        if not in_application():
            name = f.__name__
            raise Exception('Cannot call ' + name + ' in external mode')

        dataobject = convert_to_vtk_data_object(args[0])
        args = (dataobject, *args[1:])
        return f(*args, **kwargs)

    return wrapped


def with_dataset(f):
    # A decorator to automatically convert the first argument to
    # a Dataset.

    def wrapped(*args, **kwargs):
        dataset = convert_to_dataset(args[0])
        args = (dataset, *args[1:])
        return f(*args, **kwargs)

    return wrapped

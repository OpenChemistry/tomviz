# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
import inspect
import sys
import os
import fnmatch
import imp
import json
import traceback

import tomviz
import tomviz.operators


def in_application():
    return os.environ.get('TOMVIZ_APPLICATION', False)


if in_application():
    import tomviz._wrapping
    from vtk import vtkDataObject


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


def _load_module(operator_dir, python_file):
    module_name, _ = os.path.splitext(python_file)
    fp, pathname, description = imp.find_module(module_name, [operator_dir])
    module = imp.load_module(module_name, fp, pathname, description)

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


def convert_to_vtk_data_object(data):
    # This method will extract/convert certain data types to a vtkDataObject
    from tomviz.internal_dataset import Dataset

    if isinstance(data, vtkDataObject):
        # It is already a vtkDataObject
        return data

    if isinstance(data, Dataset):
        # Should be stored in _data_object
        return data._data_object

    raise Exception('Cannot convert type to vtkDataObject: ' + str(type(data)))

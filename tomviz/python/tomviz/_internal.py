# -*- coding: utf-8 -*-

###############################################################################
#
#  This source file is part of the tomviz project.
#
#  Copyright Kitware, Inc.
#
#  This source code is released under the New BSD License, (the "License").
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###############################################################################

import tomviz.operators
import tomviz._wrapping
import inspect
import sys
import os
import fnmatch
import imp
import json
import traceback

def delete_module(name):
    if name in sys.modules:
        del sys.modules[name]


def find_operator_class(transform_module):
    operator_class = None
    classes = inspect.getmembers(transform_module, inspect.isclass)
    for (name, cls) in classes:
        if issubclass(cls, tomviz.operators.Operator):
            if operator_class is not None:
                raise Exception('Multiple operators define in module, only one '
                                'operator can be defined per module.')

            operator_class = cls

    return operator_class


def find_transform_scalars_function(transform_module):
    transform_function = None
    functions = inspect.getmembers(transform_module, inspect.isfunction)
    for (name, func) in functions:
        if name == 'transform_scalars':
            transform_function = func
            break

    return transform_function


def is_cancelable(transform_module):
    cls = find_operator_class(transform_module)

    if cls is None:
        function = find_transform_scalars_function(transform_module)

    if cls is None and function is None:
        raise Exception('Unable to locate function or operator class.')

    return cls is not None and issubclass(cls,
                                          tomviz.operators.CancelableOperator)


def find_transform_scalars(transform_module, op):

    transform_function = find_transform_scalars_function(transform_module)
    if transform_function is None:
        cls = find_operator_class(transform_module)
        if cls is None:
            raise Exception('Unable to locate transform_function.')

        # We call __new__ and __init__ manually here so we can inject the
        # wrapper OperatorPython instance before __init__ is called so that
        # any code in __init__ can access the wrapper.
        o = cls.__new__(cls)
        # Set the wrapped OperatorPython instance
        o._operator_wrapper = tomviz._wrapping.OperatorPythonWrapper(op)
        cls.__init__(o)

        transform_function = o.transform_scalars

    if transform_function is None:
        raise Exception('Unable to locate transform_function.')

    return transform_function

def _load_module(operator_dir, python_file):
    module_name, _ = os.path.splitext(python_file)
    fp, pathname, description = imp.find_module(module_name, [operator_dir])
    module = imp.load_module(module_name, fp, pathname, description)

    return module

def _has_operator(module):
    return find_transform_scalars_function(module) is not None or \
            find_operator_class(module) is not None

def _operator_description(operator_dir, filename):
    name, _ = os.path.splitext(filename)
    description =  {
        'label': name,
        'pythonPath': os.path.join(operator_dir, filename),
    }

    has_operator = False
    # Load the module and see if they are valid operators
    try:
        module = _load_module(operator_dir, filename)
        has_operator = _has_operator(module)
    except:
        description['loadError'] = traceback.format_exc()

    description['valid'] = has_operator

    # See if we have a JSON file
    json_filepath = os.path.join(operator_dir, '%s.json' % name)
    if os.path.exists(json_filepath):
        description['jsonPath'] = json_filepath
        # Extract the label from the JSON
        with open(json_filepath) as fp:
            operator_json = json.load(fp)
            description['label'] = operator_json['label']

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

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

        o = cls()
        o._operator_wrapper = tomviz._wrapping.OperatorPythonWrapper(op)
        transform_function = o.transform_scalars

    if transform_function is None:
        raise Exception('Unable to locate transform_function.')

    return transform_function

import sys
import os
import pytest

import unittest

# Add fixtures to sys.path
sys.path.append(os.path.join(os.path.dirname(__file__), 'fixtures')) # noqa

import simple
import two
import function

from tomviz._internal import find_operator_class # noqa
from tomviz._internal import find_transform_from_module # noqa
from tomviz._internal import find_transform_function # noqa
from tomviz._internal import find_operators # noqa


def test_find_operator_class():

    # Module with no operators
    assert find_operator_class(unittest) is None

    # Module with an operator
    assert find_operator_class(simple) == simple.SimpleOperator

    # Module with two operator
    with pytest.raises(Exception):
        find_operator_class(two)


def test_find_transform_from_module():
    # No function
    assert find_transform_from_module(simple) is None

    # Module with a function
    found = find_transform_from_module(function)
    assert (found == function.transform_scalars)


def test_find_transform_function():
    # Module with a function
    found = find_transform_function(function)
    assert (found == function.transform_scalars)

    # Module with operator class
    func = find_transform_function(simple)
    assert isinstance(func.__self__, simple.SimpleOperator)
    assert func.__func__.__name__ == 'transform_scalars'
    assert func(None)

    # Module with none
    with pytest.raises(Exception):
        find_transform_function(unittest)


def test_find_operators():
    op_dir = os.path.join(os.path.dirname(__file__), 'fixtures')
    expected = [
        {
            'jsonPath': 'syntaxerror.json',
            'label': 'Syntax Error Op',
            'pythonPath': 'syntaxerror.py',
            'type': 'legacy_transform',
            'valid': True,
        },
        {
            'label': 'function',
            'pythonPath': 'function.py',
            'type': 'legacy_transform',
            'valid': True,
        },
        {
            'label': 'two',
            'pythonPath': 'two.py',
            'type': 'legacy_transform',
            'valid': True,
        },
        {
            'label': 'cancelable',
            'pythonPath': 'cancelable.py',
            'type': 'legacy_transform',
            'valid': True,
        },
        {
            'label': 'simple',
            'pythonPath': 'simple.py',
            'type': 'legacy_transform',
            'valid': True,
        },
        {
            'jsonPath': 'invalidjson.json',
            'label': 'invalidjson',
            'loadError': None,
            'pythonPath': 'invalidjson.py',
            'type': 'legacy_transform',
            'valid': False,
        }
    ]

    operators = find_operators(op_dir)
    for op in operators:
        if 'loadError' in op:
            op['loadError'] = None

        op['pythonPath'] = os.path.basename(op['pythonPath'])
        if 'jsonPath' in op:
            op['jsonPath'] = os.path.basename(op['jsonPath'])

    def sort_key(k):
        return k['label']

    assert (
        sorted(operators, key=sort_key) == sorted(expected, key=sort_key))

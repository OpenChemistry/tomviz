import unittest
import mock

import simple
import two
import function
import sys
import os
import tomviz


# Mock out the wrapping as the library requires symbols in tomviz
tomviz._wrapping = mock.MagicMock()
sys.modules['tomviz._wrapping'] = mock.MagicMock()
from tomviz._internal import find_operator_class # noqa
from tomviz._internal import find_transform_scalars_function # noqa
from tomviz._internal import find_transform_scalars, is_cancelable # noqa
from tomviz._internal import find_operators # noqa


class OperatorTestCase(unittest.TestCase):

    def test_find_operator_class(self):

        # Module with no operators
        self.assertIsNone(find_operator_class(unittest))

        # Module with an operator
        self.assertEqual(find_operator_class(simple), simple.SimpleOperator)

        # Module with two operator
        with self.assertRaises(Exception):
            find_operator_class(two)

    def test_find_transform_scalars_function(self):
        # No function
        self.assertIsNone(find_transform_scalars_function(simple))

        # Module with a function
        self.assertEqual(find_transform_scalars_function(
            function), function.transform_scalars)

    def test_find_transform_scalars(self):
        # Module with a function
        self.assertEqual(find_transform_scalars(
            function, None), function.transform_scalars)

        # Module with operator class
        func = find_transform_scalars(simple, None)
        self.assertTrue(isinstance(func.__self__, simple.SimpleOperator))
        self.assertEqual(func.__func__.__name__, 'transform_scalars')
        self.assertTrue(func(None))

        # Module with none
        with self.assertRaises(Exception):
            find_transform_scalars(unittest, None)

    def test_is_cancelable(self):
        # Nope
        self.assertFalse(is_cancelable(simple))
        # Nope
        self.assertFalse(is_cancelable(function))
        # Yes
        self.assertFalse(is_cancelable(function))

        with self.assertRaises(Exception):
            is_cancelable(unittest)

    def test_find_operators(self):
        op_dir = os.path.join(os.path.dirname(__file__), 'fixtures')
        #print (json.dumps(, indent=2))
        expected = [
            {
                'jsonPath': 'syntaxerror.json',
                'loadError': None,
                'label': 'Syntax Error Op',
                'valid': False,
                'pythonPath': 'syntaxerror.py'
            },
            {
                'label': 'function',
                'valid': True,
                'pythonPath': 'function.py'
            },
            {
                'loadError': None,
                'label': 'two',
                'valid': False,
                'pythonPath': 'two.py'
            },
            {
                'label': 'cancelable',
                'valid': True,
                'pythonPath': 'cancelable.py'
            },
            {
                'label': 'simple',
                'valid': True,
                'pythonPath': 'simple.py'
            },
            {
                'jsonPath': 'invalidjson.json',
                'label': 'invalidjson',
                'loadError': None,
                'pythonPath': 'invalidjson.py',
                'valid': False
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

        self.assertEqual(
            sorted(operators, key=sort_key), sorted(expected, key=sort_key))

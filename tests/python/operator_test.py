import unittest
import mock

import simple
import two
import function
import sys
import tomviz
# Mock out the wrapping as the library requires symbols in tomviz
tomviz._wrapping = mock.MagicMock()
sys.modules['tomviz._wrapping'] = mock.MagicMock()
from tomviz._internal import *

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
        self.assertEqual(find_transform_scalars_function(function), function.transform_scalars)

    def test_find_transform_scalars(self):
        # Module with a function
        self.assertEqual(find_transform_scalars(function, None), function.transform_scalars)

        # Module with operator class
        func = find_transform_scalars(simple, None)

        self.assertTrue(isinstance(func.im_self, simple.SimpleOperator))
        self.assertEqual(func.im_func.__name__, 'transform_scalars')
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

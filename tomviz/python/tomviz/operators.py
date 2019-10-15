# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################


class Progress(object):
    """
    Class used to update operator progress.
    """

    def __init__(self, operator):
        self._operator = operator

    @property
    def maximum(self):
        """
        Property defining the maximum progress value
        """
        return self._operator._operator_wrapper.progress_maximum

    @maximum.setter
    def maximum(self, value):
        self._operator._operator_wrapper.progress_maximum = value

    @property
    def value(self):
        """
        Property defining the current progress value
        """
        return self._operator._operator_wrapper.progress_value

    @value.setter
    def value(self, value):
        """
        Updates the progress of the the operator.

        :param value The current progress value.
        :type value: int
        """
        self._operator._operator_wrapper.progress_value = value

    @property
    def message(self):
        """
        Property defining the current progress message
        """
        return self._operator._operator_wrapper.progress_message

    @message.setter
    def message(self, msg):
        """
        Updates the progress message of the the operator.

        :param msg The current progress message.
        :type msg: str
        """
        self._operator._operator_wrapper.progress_message = msg

    def _data(self, value):
        # Make sure this is a vtkDataObject before setting
        from tomviz._internal import convert_to_vtk_data_object
        data = convert_to_vtk_data_object(value)
        self._operator._operator_wrapper.progress_data = data

    # Write-only property to update child data
    data = property(fset=_data)


class Operator(object):
    """
    The base operator class from which all operators should be derived.
    """
    def __new__(cls, *args, **kwargs):
        obj = super(Operator, cls).__new__(cls)
        obj.progress = Progress(obj)

        return obj

    def transform_scalars(self, data):
        """
        This method should be overriden by subclasses to implement the
        operations the operator should perform.
        """
        raise NotImplementedError('Must be implemented by subclass')

    def transform(self, data):
        """
        This method should be overriden by subclasses to implement the
        operations the operator should perform.
        """
        raise NotImplementedError('Must be implemented by subclass')


class CancelableOperator(Operator):
    """
    A cancelable operator allows the user to interrupt the execution of the
    operator. The canceled property can be using in the transform(...)
    method to break out when the operator is canceled. The basic structure of
    the transform(...) might look something like this:

    def transform(self, data):
        while(not self.canceled):
            # Do work

    """
    @property
    def canceled(self):
        """
        :returns True if the operator has been canceled, False otherwise.
        """
        return self._operator_wrapper.canceled

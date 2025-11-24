# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
from .dataset import Dataset


class Progress:
    """
    Class used to update operator progress.

    This often exists as `self.progress` on an `Operator` class.

    An example of how it can be utilized within a `transform()` function
    is provided below:

    .. code-block:: python

        class MyOperator(Operator):
            def transform(self, dataset, ...):
                self.progress.maximum = 100

                for i in range(100):
                    self.progress.value = i
                    self.progress.message = f'Running: {i}'
    """

    def __init__(self, operator):
        """
        :meta private:
        """
        self._operator = operator

    @property
    def maximum(self) -> int:
        """
        Property defining the maximum progress value
        """
        return self._operator._operator_wrapper.progress_maximum

    @maximum.setter
    def maximum(self, value: int):
        """Set the maximum progress value"""
        self._operator._operator_wrapper.progress_maximum = value

    @property
    def value(self) -> int:
        """
        Property defining the current progress value
        """
        return self._operator._operator_wrapper.progress_value

    @value.setter
    def value(self, value: int):
        """Updates the progress of the the operator"""
        self._operator._operator_wrapper.progress_value = value

    @property
    def message(self) -> str:
        """Property defining the current progress message"""
        return self._operator._operator_wrapper.progress_message

    @message.setter
    def message(self, msg: str):
        """Update the progress message of the the operator"""
        self._operator._operator_wrapper.progress_message = msg

    def _data(self, value):
        from tomviz._internal import in_application
        if in_application():
            # Make sure this is a vtkDataObject before setting
            from tomviz._internal import convert_to_vtk_data_object
            value = convert_to_vtk_data_object(value)

        self._operator._operator_wrapper.progress_data = value

    # Write-only property to update child data
    data = property(fset=_data,
                    doc="""
                    :meta private:
                    """)


class Operator:
    """
    The base operator class from which all operators should be derived.

    Progress can be utilized and modified via the `self.progress` object.
    Details about the `self.progress` object interface can be see in the
    `tomviz.operators.Progress` class.
    """
    def __new__(cls, *args, **kwargs):
        """
        :meta private:
        """
        obj = super(Operator, cls).__new__(cls)
        obj.progress = Progress(obj)

        return obj

    def transform_scalars(self, data):
        """
        This method should be overridden by subclasses to implement the
        operations the operator should perform.

        :meta private:
        """
        raise NotImplementedError('Must be implemented by subclass')

    def transform(self, dataset: Dataset, *args: tuple,
                  **kwargs: dict) -> dict | None:
        """
        Apply transformations to the dataset in order to obtain the
        desired output.

        Typically, the arrays on the dataset are modified in place,
        and no return is necessary. However, in situations like 3D
        reconstructions, a dictionary should be returned with
        a key such as "reconstruction" and a value of a child dataset
        containing the reconstruction.

        :meta private:
        """
        raise NotImplementedError('Must be implemented by subclass')


class CancelableOperator(Operator):
    """
    A cancelable operator allows the user to interrupt the execution of the
    operator. The canceled property can be using in the transform(...)
    method to break out when the operator is canceled.

    To utilize this class, the `transform()` function must be defined
    as a method within a class that inherits from `Progress` within the
    operator module. For example:

    .. code-block:: python

        import tomviz.operators

        class MyCancelableOperator(tomviz.operators.CancelableOperator):
            def transform(self, dataset, ...):
                while not self.canceled:
                    # Do work

    Note how `self.canceled` is checked in order to determine whether the
    operator was canceled or not. If it was canceled, the `transform()`
    function should abort.

    This operator may also report progress as well in `self.progress`.
    See the generic `Operator` class for details.
    """
    @property
    def canceled(self) -> bool:
        """
        :returns True if the operator has been canceled, False otherwise.
        """
        return self._operator_wrapper.canceled


class CompletableOperator(CancelableOperator):
    """
    A completable operator allows a user to interrupt the execution of
    the operator using either "cancel" or "complete". The
    completable property can be used in the transform(...) method to break out
    when an operator is finished early, like if an iterative algorithm is a
    reasonable quality before the designated iterations are reached. Use
    similar to "cancel", but be sure to return data.

    To utilize this class, the `transform()` function must be defined
    as a method within a class that inherits from `CompletableOperator`
    within the operator module. For example:

    .. code-block:: python

        import tomviz.operators

        class MyCompletableOperator(tomviz.operators.CompletableOperator):
            def transform(self, dataset, ...):
                while True:
                    if self.completed:
                        # Break out of the iterative loop and return the current
                        # results.

    Note how `self.completed` is checked in order to determine whether the
    operator was completed or not. If completed, the transform function
    should skip to the end and return the current dataset.

    `self.canceled` may also be used in this operator, since it inherits
    from `CancelableOperator` as well.

    This operator may also report progress as well in `self.progress`.
    See the generic `Operator` class for details.
    """
    @property
    def completed(self) -> bool:
        """
        :returns True if the operator is early completed (from Button),
        False otherwise
        """
        return self._operator_wrapper.completed

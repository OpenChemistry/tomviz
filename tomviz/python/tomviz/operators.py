class Operator:
    def transform_scalars(self, data):
        raise NotImplementedError('Must be implemented by subclass')

class CancelableOperator(Operator):

    @property
    def canceled(self):
        return self._operator_wrapper.canceled


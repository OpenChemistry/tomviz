import tomviz.operators

class TestOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, data):
        if self.canceled:
            raise Exception('Quick let get out of here!')


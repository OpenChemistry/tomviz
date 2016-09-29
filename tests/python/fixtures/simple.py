import tomviz.operators

class SimpleOperator(tomviz.operators.Operator):
    def transform_scalars(self, data):
        return True

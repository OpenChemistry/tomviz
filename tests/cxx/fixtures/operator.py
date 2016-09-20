import tomviz

class TestOperator(tomviz.Operator):

    def transform_scalars(self, data):
        return True

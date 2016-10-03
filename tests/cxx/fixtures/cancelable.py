import tomviz.operators
import time


class TestOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, data):
        i = 0
        while not self.canceled and i < 5:
            time.sleep(0.1)
            i += 1

        if self.canceled:
            raise Exception('Quick let get out of here!')

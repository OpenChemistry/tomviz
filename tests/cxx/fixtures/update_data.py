import tomviz.operators
from vtk import vtkImageData, VTK_DOUBLE


class TestOperator(tomviz.operators.Operator):

    def transform_scalars(self, data):
        image_data = vtkImageData()
        image_data.SetDimensions(3, 4, 5)
        image_data.AllocateScalars(VTK_DOUBLE, 1)
        dims = image_data.GetDimensions()

        for z in range(dims[2]):
            for y in range(dims[1]):
                for x in range(dims[0]):
                    image_data.SetScalarComponentFromDouble(x, y, z, 0, 2.0)
        self.progress.data = image_data

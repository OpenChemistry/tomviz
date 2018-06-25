import numpy as np
import vtk
import tomviz.operators
import tomviz.utils

class DummyMoleculeOperator(tomviz.operators.CancelableOperator):

    def transform_scalars(self, dataset):
        """Reconstruct atomic positions"""
        
        atomic_numbers = [ 6, 1, 6, 1, 6, 1, 6, 1, 6, 1, 6, 1 ]

        positions = [
            -0.9853672723415879,
            0.9853672723415879,
            0.0,
            -1.765101316934915,
            1.765101316934915,
            0.0,
            -1.346036726076389,
            -0.3606694537348005,
            0.0,
            -2.411173239186462,
            -0.6460719222515465,
            0.0,
            -0.3606694537348005,
            -1.346036726076389,
            0.0,
            -0.6460719222515465,
            -2.411173239186462,
            0.0,
            0.9853672723415879,
            -0.9853672723415879,
            0.0,
            1.765101316934915,
            -1.765101316934915,
            0.0,
            1.346036726076389,
            0.3606694537348005,
            0.0,
            2.411173239186462,
            0.6460719222515465,
            0.0,
            0.3606694537348005,
            1.346036726076389,
            0.0,
            0.6460719222515465,
            2.411173239186462,
            0.0
        ]

        bonds = [
            0,
            1,
            0,
            2,
            0,
            10,
            2,
            3,
            2,
            4,
            4,
            5,
            4,
            6,
            6,
            7,
            6,
            8,
            8,
            9,
            8,
            10,
            10,
            11
        ]

        bonds_order = [ 1, 1, 2, 1, 2, 1, 1, 1, 2, 1, 1, 1 ]
        
        data = tomviz.utils.get_array(dataset).astype(float)
        molecule = vtk.vtkMolecule()
        for i in range(len(atomic_numbers)):
            pos = np.array(positions[i * 3 : i * 3 + 3])
            pos *= 30
            pos += [150, 150, 150]
            molecule.AppendAtom(atomic_numbers[i], pos[0], pos[1], pos[2])

        for i in range(len(bonds_order)):
            molecule.AppendBond(bonds[i * 2], bonds[i * 2 + 1], bonds_order[i])

        return {"molecule": molecule}


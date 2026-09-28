import numpy as np
import tomviz.operators
import tomviz.utils


class DummyMoleculeOperator(tomviz.operators.CancelableOperator):

    def transform(self, dataset):
        """Emit a benzene molecule at a fixed position"""

        atomic_numbers = [6, 1, 6, 1, 6, 1, 6, 1, 6, 1, 6, 1]

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
            0, 1,
            0, 2,
            0, 10,
            2, 3,
            2, 4,
            4, 5,
            4, 6,
            6, 7,
            6, 8,
            8, 9,
            8, 10,
            10, 11
        ]

        bonds_order = [1, 1, 2, 1, 2, 1, 1, 1, 2, 1, 1, 1]

        positions = np.asarray(positions).reshape(-1, 3) + [150, 150, 40]

        molecule = tomviz.utils.make_molecule(atomic_numbers, positions,
                                              bonds, bonds_order)

        return {"molecule": molecule}

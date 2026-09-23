import time
from typing import Any

import numpy as np
import scipy.ndimage

import tomviz.nodes
from tomviz.dataset import Dataset


class SimulatedLiveAcquisition(tomviz.nodes.SourceNode):
    """A pretend tomography scan: one new projection every few seconds.
    Copy it to make a source that watches a real instrument."""

    def produce(self, size: int = 64, num_projections: int = 60,
                start_angle: float = -60.0, end_angle: float = 60.0,
                seconds_per_projection: float = 5.0,
                acquired: int = 0) -> dict[str, Dataset] | None:
        # Build the dataset from every projection recorded so far
        if 'started_at' not in self.state:
            self.state['started_at'] = time.time()  # the scan starts now
        count = self._recorded(num_projections, seconds_per_projection)
        angles = np.linspace(start_angle, end_angle, num_projections)[:count]

        sample = self._test_object(size)
        projections = np.empty((size, size, count), np.float32, order='F')
        self.progress.maximum = count
        for i, angle in enumerate(angles):
            if self.canceled:
                return None
            rotated = scipy.ndimage.rotate(sample, -angle, axes=(1, 2),
                                           reshape=False, order=1)
            projections[:, :, i] = rotated.sum(axis=2)
            self.progress.value = i + 1

        self.set_parameter('acquired', count)  # shown in the dialog

        dataset = self.create_dataset()
        dataset.set_scalars('Projections', projections)
        dataset.tilt_angles = angles
        return {'tilt_series': dataset}

    def should_auto_execute(self, **params: Any) -> bool:
        # Called every interval: True re-runs the pipeline. Keep it cheap.
        if 'started_at' not in self.state:
            # self.state is not saved: after loading a state file, re-run
            return params['acquired'] > 0
        count = self._recorded(params['num_projections'],
                               params['seconds_per_projection'])
        return count > params['acquired']

    def _recorded(self, num_projections: int,
                  seconds_per_projection: float) -> int:
        # How many projections the pretend instrument has recorded by now
        elapsed = time.time() - self.state['started_at']
        return min(1 + int(elapsed // seconds_per_projection),
                   num_projections)

    @staticmethod
    def _test_object(size: int) -> np.ndarray:
        # A sphere with a 3D sine wave inside
        r = np.linspace(-1, 1, size)
        x, y, z = np.meshgrid(r, r, r, indexing='ij')
        wave = 1 + 0.5 * np.sin(2 * np.pi * x) * np.sin(2 * np.pi * y) * \
            np.sin(2 * np.pi * z)
        sphere = x**2 + y**2 + z**2 < 0.8**2
        return np.where(sphere, wave, 0).astype(np.float32)

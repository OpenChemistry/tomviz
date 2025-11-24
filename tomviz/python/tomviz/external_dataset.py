import collections.abc
import copy

import numpy as np

from tomviz.dataset import Dataset as AbstractDataset

ARRAY_TYPES = (collections.abc.Sequence, np.ndarray)


class Dataset(AbstractDataset):
    def __init__(self, arrays, active=None):
        # Holds the map of scalars name => array
        self.arrays = arrays
        self.tilt_angles = None
        self.tilt_axis = None
        # The currently active scalar
        self.active_name = active
        # If we weren't given the active array, set the first as the active
        # array.
        if active is None and len(arrays.keys()):
            self.active_name = next(iter(arrays.keys()))

        self._spacing = None

        # Dark and white backgrounds
        self.dark = None
        self.white = None

        # Filename and metadata
        self.file_name = None
        self.metadata = {}

    @property
    def active_scalars(self):
        return self.scalars(self.active_name)

    @active_scalars.setter
    def active_scalars(self, array):
        self.set_scalars(self.active_name, array)

    @property
    def num_scalars(self):
        return len(self.arrays)

    @property
    def scalars_names(self):
        return list(self.arrays.keys())

    def scalars(self, name=None):
        if name is None:
            name = self.active_name
        return self.arrays[name]

    def set_scalars(self, name, array):
        self.arrays[name] = array

    @property
    def spacing(self):
        return self._spacing

    @spacing.setter
    def spacing(self, v):
        if not isinstance(v, ARRAY_TYPES):
            raise Exception('Spacing must be an iterable type')
        if not len(v) == 3:
            raise Exception('Length of spacing must be 3')

        self._spacing = v

    @property
    def active_name(self) -> str:
        return self._active_name

    @active_name.setter
    def active_name(self, v: str):
        self._active_name = v

    @property
    def tilt_axis(self) -> int | None:
        return self._tilt_axis

    @tilt_axis.setter
    def tilt_axis(self, v: int | None):
        self._tilt_axis = v

    @property
    def tilt_angles(self) -> np.ndarray | None:
        return self._tilt_angles

    @tilt_angles.setter
    def tilt_angles(self, v: np.ndarray | None):
        self._tilt_angles = v

    @property
    def file_name(self) -> str | None:
        return self._file_name

    @file_name.setter
    def file_name(self, v: str | None):
        self._file_name = v

    @property
    def metadata(self) -> dict:
        return self._metadata

    @metadata.setter
    def metadata(self, v: dict):
        self._metadata = v

    @property
    def dark(self) -> np.ndarray | None:
        return self._dark

    @dark.setter
    def dark(self, v: np.ndarray | None):
        self._dark = v

    @property
    def white(self) -> np.ndarray | None:
        return self._white

    @white.setter
    def white(self, v: np.ndarray | None):
        self._white = v

    def create_child_dataset(self):
        child = copy.deepcopy(self)
        # Set tilt angles to None to be consistent with internal dataset
        child.tilt_angles = None
        # If the parent had tilt angles, set the spacing of the tilt
        # axis to match that of x, as is done in the internal dataset
        if self.tilt_angles is not None and self.spacing is not None:
            s = self.spacing
            child.spacing = [s[0], s[1], s[0]]
        return child

    def rename_active(self, new_name: str):
        self.arrays[new_name] = self.arrays.pop(self.active_name)

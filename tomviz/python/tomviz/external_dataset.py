import collections
import copy

import numpy as np

ARRAY_TYPES = (collections.Sequence, np.ndarray)


class Dataset:
    def __init__(self, arrays, active=None):
        # Holds the map of scalars name => array
        self.arrays = arrays
        self.tilt_angles = None
        self.tilt_axis = None
        # The currently active scalar
        self.active_name = active
        # If we weren't given the active array and we only have one array, set
        # it as the active array.
        if active is None and len(arrays.keys()):
            (self.active_name,) = arrays.keys()

        self._spacing = None

        # Dark and white backgrounds
        self.dark = None
        self.white = None

    @property
    def active_scalars(self):
        return self.arrays[self.active_name]

    @active_scalars.setter
    def active_scalars(self, array):
        if self.active_name is None:
            self.active_name = 'Scalars'
        self.set_scalars(self.active_name, array)

    @property
    def scalars_names(self):
        return list(self.arrays.keys())

    def scalars(self, name=None):
        if name is None:
            name = self.active_name
        return self.arrays[name]

    def set_scalars(self, name, array):
        if self.active_name is None:
            self.active_name = name

        # Throw away any arrays whose sizes don't match
        for n, a in list(self.arrays.items()):
            if n != name and a.shape != array.shape:
                print('Warning: deleting array', n, 'because its shape',
                      a.shape, 'does not match the shape of new array', name,
                      array.shape)
                del self.arrays[n]

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

    def create_child_dataset(self, volume=True):
        # Create an empty dataset with the same spacing as the parent
        child = Dataset({})

        child.spacing = copy.deepcopy(self.spacing)

        if volume:
            if self.tilt_axis is not None:
                # Ignore the tilt angle spacing. Set it to another spacing.
                child.spacing[self.tilt_axis] = child.spacing[1]
        else:
            child.tilt_angles = copy.deepcopy(self.tilt_angles)
            child.tilt_axis = self.tilt_axis

        return child

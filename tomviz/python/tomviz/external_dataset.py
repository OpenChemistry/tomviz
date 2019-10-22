import copy


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

        # Dark and white backgrounds
        self.dark = None
        self.white = None

    @property
    def active_scalars(self):
        return self.arrays[self.active_name]

    @active_scalars.setter
    def active_scalars(self, array):
        self.arrays[self.active_name] = array

    @property
    def scalars_names(self):
        return list(self.arrays.keys())

    def scalars(self, name=None):
        if name is None:
            name = self.active_name
        return self.arrays[name]

    def create_child_dataset(self):
        child = copy.deepcopy(self)
        # Set tilt angles to None to be consistent with internal dataset
        child.tilt_angles = None
        return child

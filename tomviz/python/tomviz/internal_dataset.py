from tomviz import utils


class Dataset:
    def __init__(self, data_object, data_source):
        self._data_object = data_object
        self._data_source = data_source

        self._tilt_axis = 2

    @property
    def active_scalars(self):
        return utils.get_array(self._data_object)

    @active_scalars.setter
    def active_scalars(self, v):
        utils.set_array(self._data_object, v)

    @property
    def scalars_names(self):
        return utils.array_names(self._data_object)

    def scalars(self, name=None):
        return utils.get_array(self._data_object, name)

    def set_scalars(self, name, array):
        utils.set_array(self._data_object, array, name=name)

    @property
    def spacing(self):
        return utils.get_spacing(self._data_object)

    @spacing.setter
    def spacing(self, v):
        utils.set_spacing(self._data_object, v[0], v[1], v[2])

    @property
    def tilt_angles(self):
        return utils.get_tilt_angles(self._data_object)

    @tilt_angles.setter
    def tilt_angles(self, v):
        utils.set_tilt_angles(self._data_object, v)

    @property
    def tilt_axis(self):
        return self._tilt_axis

    @tilt_axis.setter
    def tilt_axis(self, v):
        self._tilt_axis = v

    @property
    def dark(self):
        if not self._data_source.dark_data:
            return None
        return utils.get_array(self._data_source.dark_data)

    @property
    def white(self):
        if not self._data_source.white_data:
            return None
        return utils.get_array(self._data_source.white_data)

    def create_child_dataset(self, volume=True):
        new_data = utils.make_child_dataset(self._data_object, volume)
        return Dataset(new_data, self._data_source)


def create_dataset(data_object, data_source):
    return Dataset(data_object, data_source)

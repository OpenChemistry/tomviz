from tomviz import internal_utils
from tomviz.dataset import Dataset as AbstractDataset


class Dataset(AbstractDataset):
    def __init__(self, data_object, data_source):
        self._data_object = data_object
        self._data_source = data_source

        self._tilt_axis = 2

    @property
    def active_scalars(self):
        return internal_utils.get_array(self._data_object)

    @active_scalars.setter
    def active_scalars(self, v):
        internal_utils.set_array(self._data_object, v)

    @property
    def active_name(self):
        return internal_utils.get_active_scalars_name(self._data_object)

    @property
    def num_scalars(self):
        return len(self.scalars_names)

    @property
    def scalars_names(self):
        return internal_utils.array_names(self._data_object)

    def scalars(self, name=None):
        return internal_utils.get_array(self._data_object, name)

    def set_scalars(self, name, array):
        internal_utils.set_array(self._data_object, array, name=name)

    @property
    def spacing(self):
        return internal_utils.get_spacing(self._data_object)

    @spacing.setter
    def spacing(self, v):
        internal_utils.set_spacing(self._data_object, v[0], v[1], v[2])

    @property
    def tilt_angles(self):
        return internal_utils.get_tilt_angles(self._data_object)

    @tilt_angles.setter
    def tilt_angles(self, v):
        internal_utils.set_tilt_angles(self._data_object, v)

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
        return internal_utils.get_array(self._data_source.dark_data)

    @property
    def white(self):
        if not self._data_source.white_data:
            return None
        return internal_utils.get_array(self._data_source.white_data)

    @property
    def file_name(self):
        return self._data_source.file_name

    @property
    def metadata(self):
        return self._data_source.metadata

    def create_child_dataset(self):
        new_data = internal_utils.make_child_dataset(self._data_object)
        return Dataset(new_data, self._data_source)

    def rename_active(self, new_name: str):
        scalars = self._data_object.GetPointData().GetScalars()
        scalars.SetName(new_name)


def create_dataset(data_object, data_source):
    return Dataset(data_object, data_source)

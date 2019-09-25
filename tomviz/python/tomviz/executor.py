import h5py
import importlib
import os
import numpy as np
import logging
import tempfile
import socket
import abc
import stat
import json
import six
import errno

from tqdm import tqdm

from tomviz import utils
from tomviz._internal import find_transform_function

LOG_FORMAT = '[%(asctime)s] %(levelname)s: %(message)s'

logger = logging.getLogger('tomviz')
logger.setLevel(logging.INFO)
stream_handler = logging.StreamHandler()
formatter = logging.Formatter(LOG_FORMAT)
stream_handler.setFormatter(formatter)
logger.addHandler(stream_handler)

DIMS = ['dim1', 'dim2', 'dim3']
ANGLE_UNITS = [b'[deg]', b'[rad]']


class ProgressBase(object):
    def started(self, op=None):
        self._operator_index = op

    def finished(self, op=None):
        pass


class TqdmProgress(ProgressBase):
    """
    Class used to update operator progress. This replaces the C++ bound one that
    is used inside the Qt applicaition.
    """

    def __init__(self):
        self._maximum = None
        self._value = None
        self._message = None
        self._progress_bar = None

    @property
    def maximum(self):
        """
        Property defining the maximum progress value
        """
        return self._maximum

    @maximum.setter
    def maximum(self, value):
        self._progress_bar = tqdm(total=value)
        self._maximum = value

    @property
    def value(self):
        """
        Property defining the current progress value
        """
        return self._value

    @value.setter
    def value(self, value):
        """
        Updates the progress of the the operator.

        :param value The current progress value.
        :type value: int
        """
        self._progress_bar.update(value - self._progress_bar.n)
        self._value = value

    @property
    def message(self):
        """
        Property defining the current progress message
        """
        return self._message

    @message.setter
    def message(self, msg):
        """
        Updates the progress message of the the operator.

        :param msg The current progress message.
        :type msg: str
        """
        self._progress_bar.set_description(msg)
        self._message = msg

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if self._progress_bar is not None:
            self._progress_bar.close()

        return False

    def finished(self, op=None):
        if self._progress_bar is not None:
            self._progress_bar.close()


class JsonProgress(ProgressBase, metaclass=abc.ABCMeta):
    """
    Abstract class used to update operator progress using JSON based messages.
    """

    @abc.abstractmethod
    def write(self, data):
        """
        Method the write JSON progress message. Implemented by subclass.
        """

    @abc.abstractmethod
    def write_to_file(self, data):
        """
        Write data to write and return path. Implemented by subclass.
        """

    def set_operator_index(self, index):
        self._operator_index = index

    @property
    def maximum(self):
        """
        Property defining the maximum progress value
        """
        return self._maximum

    @maximum.setter
    def maximum(self, value):
        msg = {
            'type': 'progress.maximum',
            'operator': self._operator_index,
            'value': value
        }
        self.write(msg)
        self._maximum = value

    @property
    def value(self):
        """
        Property defining the current progress value
        """
        return self._value

    @value.setter
    def value(self, value):
        """
        Updates the progress of the the operator.

        :param value The current progress value.
        :type value: int
        """
        msg = {
            'type': 'progress.step',
            'operator': self._operator_index,
            'value': value
        }
        self.write(msg)

        self._value = value

    @property
    def message(self):
        """
        Property defining the current progress message
        """
        return self._message

    @message.setter
    def message(self, msg):
        """
        Updates the progress message of the the operator.

        :param msg The current progress message.
        :type msg: str
        """
        m = {
            'type': 'progress.message',
            'operator': self._operator_index,
            'value': msg
        }
        self.write(m)

        self._message = msg

    @property
    def data(self):
        return self._data

    @data.setter
    def data(self, value):
        """
        Updates the progress of the the operator.

        :param data The current progress data value.
        :type value: numpy.ndarray
        """
        path = self.write_to_file(value)
        msg = {
            'type': 'progress.data',
            'operator': self._operator_index,
            'value': path
        }
        self.write(msg)
        self._data = value

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def started(self, op=None):
        super(JsonProgress, self).started(op)
        msg = {
            'type': 'started'
        }

        if op is not None:
            msg['operator'] = op

        self.write(msg)

    def finished(self, op=None):
        super(JsonProgress, self).started(op)
        msg = {
            'type': 'finished'
        }

        if op is not None:
            msg['operator'] = op

        self.write(msg)


class WriteToFileMixin(object):
    def write_to_file(self, dataobject):
        filename = '%d.emd' % self._sequence_number
        path = os.path.join(os.path.dirname(self._path), filename)
        _write_emd(path, dataobject)
        self._sequence_number += 1

        return filename


class LocalSocketProgress(WriteToFileMixin, JsonProgress):
    """
    Class used to update operator progress. Connects to QLocalServer and writes
    JSON message updating the UI on pipeline progress.
    """

    def __init__(self, socket_path):
        self._maximum = None
        self._value = None
        self._message = None
        self._connection = None
        self._path = socket_path
        self._sequence_number = 0

        try:
            mode = os.stat(self._path).st_mode
            if stat.S_ISSOCK(mode):
                self._uds_connect()
            else:
                raise Exception('Invalid progress path type.')

        except OSError:
            raise Exception('TqdmProgress path doesn\'t exist')

    def _uds_connect(self):
        self._connection = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self._connection.connect(self._path)

    def write(self, data):
        data = ('%s\n' % json.dumps(data)).encode('utf8')
        if isinstance(self._connection, socket.socket):
            self._connection.sendall(data)
        else:
            self._connection.write(data)

    def __exit__(self, *exc):
        if self._connection is not None:
            self._connection.close()

        return False


class FilesProgress(WriteToFileMixin, JsonProgress):
    """
    Class used to update operator progress by writing a sequence of files to a
    directory.
    """
    def __init__(self, path):
        self._path = path
        self._sequence_number = 0

    def write(self, data):
        file_path = os.path.join(self._path,
                                 'progress%d' % self._sequence_number)
        self._sequence_number += 1

        with open(file_path, 'w') as f:
            json.dump(data, f)

        return filename


def _progress(progress_method, progress_path):
    if progress_method == 'tqdm':
        return TqdmProgress()
    elif progress_method == 'socket':
        return LocalSocketProgress(progress_path)
    elif progress_method == 'files':
        return FilesProgress(progress_path)
    else:
        raise Exception('Unrecognized progress method: %s' % progress_method)


class OperatorWrapper(object):
    canceled = False


def _load_operator_module(label, script):
    # Load the operator module, we write the code to a temporary file before
    # using importlib to do the loading ( couldn't figure a way to directly
    # load a module from a string).
    fd = None
    path = None

    try:
        (fd, path) = tempfile.mkstemp(suffix='.py', text=True)
        os.write(fd, script.encode())
        os.close(fd)
        fd = None

        spec = importlib.util.spec_from_file_location(label, path)
        operator_module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(operator_module)

    finally:
        if fd is not None:
            os.close(fd)
        if path is not None:
            os.unlink(path)

    return operator_module


def _read_dataset(dataset, options=None):
    if options is None or 'subsampleSettings' not in options:
        return dataset[:]

    stride = options.get('subsampleSettings', {}).get('stride', 1)
    bnds = options.get('subsampleSettings', {}).get('volumeBounds', [-1] * 6)

    if len(bnds) != 6 or any(x < 0 for x in bnds):
        # Set the default bounds
        for i in range(3):
            bnds[i * 2] = 0
            bnds[i * 2 + 1] = dataset.shape[i]

    # These slice specifications are forwarded directly to the HDF5
    # "hyperslab" selections, and it loads only the data requested.
    return dataset[bnds[0]:bnds[1]:stride,
                   bnds[2]:bnds[3]:stride,
                   bnds[4]:bnds[5]:stride]


def _read_emd(path, options=None):
    with h5py.File(path, 'r') as f:
        tomography = f['data/tomography']

        dims = []
        for dim in DIMS:
            dims.append((dim,
                         tomography[dim][:],
                         tomography[dim].attrs['name'][0],
                         tomography[dim].attrs['units'][0]))

        data = tomography['data']
        # We default the name to ImageScalars
        name = data.attrs.get('name', 'ImageScalars')
        if not isinstance(name, str):
            name = name[0].decode()

        arrays = [(name, _read_dataset(data, options))]

        # See if we have any additional channels
        tomviz_scalars = tomography.get('tomviz_scalars')
        if isinstance(tomviz_scalars, h5py.Group):
            # Get the datasets
            channel_datasets = [(name, _read_dataset(dataset, options))
                                for (name, dataset) in tomviz_scalars.items()]
            arrays += channel_datasets

        # If this is a tilt series, swap the X and Z axes
        tilt_axis = None
        if dims[0][2] == b'angles' or dims[0][3] in ANGLE_UNITS:
            arrays = [(name, np.transpose(data, [2, 1, 0])) for (name, data)
                      in arrays]

            # Swap the dims order as well
            dims[0], dims[-1] = dims[-1], dims[0]
            tilt_axis = 2

        # EMD stores data as row major order.  VTK expects column major order.
        arrays = [(name, np.asfortranarray(data)) for (name, data) in arrays]

        output = {
            'arrays': arrays,
            'dims': dims,
            'tilt_axis': tilt_axis
        }

        if dims is not None and dims[-1][2] == b'angles':
            output['tilt_angles'] = dims[-1][1][:].astype(np.float64)

        return output


def _write_emd(path, dataobject, dims=None):
    tilt_angles = dataobject.tilt_angles
    tilt_axis = dataobject.tilt_axis
    active_array = dataobject.active
    # Separate out the extra channels/arrays as we store them separately
    extra_arrays = {name: array for name, array
                    in dataobject.arrays.items()
                    if id(array) != id(active_array)}

    # If this is a tilt series, swap the X and Z axes
    if tilt_angles is not None and tilt_axis == 2:
        active_array = np.transpose(active_array, [2, 1, 0])
        extra_arrays = {name: np.transpose(array, [2, 1, 0]) for (name, array)
                        in extra_arrays.items()}

    # Switch back to row major order for EMD stores
    active_array = np.ascontiguousarray(active_array)
    extra_arrays = {name: np.ascontiguousarray(array) for (name, array)
                    in extra_arrays.items()}

    with h5py.File(path, 'w') as f:
        f.attrs.create('version_major', 0, dtype='uint32')
        f.attrs.create('version_minor', 2, dtype='uint32')
        data_group = f.create_group('data')
        tomography_group = data_group.create_group('tomography')
        tomography_group.attrs.create('emd_group_type', 1, dtype='uint32')
        data = tomography_group.create_dataset('data', data=active_array)
        data.attrs['name'] = np.string_(dataobject.active_name)

        if dims is None:
            dims = []
            names = ['x', 'y', 'z']
            if tilt_angles is not None:
                names = ['angles', 'y', 'x']

            for i, name in zip(range(0, 3), names):
                values = np.array(range(data.shape[i]))
                units = '[n_m]' if name != 'angles' else '[deg]'
                dims.append(('/data/tomography/dim%d' % (i+1),
                             values, name, units))
        else:
            # Swap the dims as well
            if tilt_angles is not None and tilt_axis == 2:
                dims[0], dims[-1] = dims[-1], dims[0]

        # add dimension vectors
        for (dataset_name, values, name, units) in dims:
            d = tomography_group.create_dataset(dataset_name, values.shape)
            d.attrs['name'] = np.string_(name)
            d.attrs['units'] = np.string_(units)
            d[:] = values

        # If we have angle update the first dim
        if tilt_angles is not None:
            (name, _, _, units) = dims[0]
            d = tomography_group[name]
            d.attrs['name'] = np.string_('angles')
            # Only override we don't already have a valid angle units.
            if units not in ANGLE_UNITS:
                d.attrs['units'] = np.string_('[deg]')
            d[:] = tilt_angles

        # If we have extra scalars add them under tomviz_scalars
        if extra_arrays:
            tomviz_scalars = tomography_group.create_group('tomviz_scalars')
            for (name, array) in extra_arrays.items():
                tomviz_scalars.create_dataset(name, data=array)


def _read_data_exchange(path, options=None):
    with h5py.File(path, 'r') as f:
        g = f['/exchange']

        dataset_names = ['data', 'data_dark', 'data_white', 'theta']
        datasets = {}
        for name in dataset_names:
            if name in g:
                # Don't read theta with subsample options
                if name == 'theta':
                    datasets[name] = g[name][:]
                    continue

                datasets[name] = _read_dataset(g[name], options)

        # Data Exchange stores data as row major order.
        # VTK expects column major order.
        if options is None:
            to_fortran = True
        else:
            to_fortran = not options.get('keep_c_ordering', False)

        if to_fortran:
            datasets = { name: np.asfortranarray(data)
                         for (name, data) in datasets.items() }

            if 'theta' in datasets:
                # Swap x and z axes
                swap_keys = ['data', 'data_dark', 'data_white']
                for key in swap_keys:
                    datasets[key] = np.transpose(datasets[key], [2, 1, 0])

        tilt_axis = None
        if 'theta' in datasets:
            tilt_axis = 2 if to_fortran else 0

        output = {
            'arrays': [('data', datasets.get('data'))],
            'data_dark': datasets.get('data_dark'),
            'data_white': datasets.get('data_white'),
            'tilt_angles': datasets.get('theta'),
            'tilt_axis': tilt_axis
        }

        return output


def _is_data_exchange(path):
    # It must have an HDF5 extension
    exts = ['.h5', '.hdf5']
    if not any([path.suffix.lower() == x for x in exts]):
        return False

    # Open it up and make sure /exchange/data exists
    with h5py.File(path, 'r') as f:
        return '/exchange/data' in f


class DataObject(object):
    def __init__(self, arrays, active=None):
        # Holds the map of scalars name => array
        self.arrays = arrays
        self.is_volume = False
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

    def set_scalars(self, new_scalars):
        order = 'C'
        if np.isfortran(self.active):
            order = 'F'
        new_scalars = np.reshape(new_scalars, self.active.shape, order=order)
        self.active = np.asfortranarray(new_scalars)

    def get_scalars(self, name=None):
        if name is None:
            name = self.active_name
        return self.arrays[name].ravel(order='A')

    def set_array(self, new_array):
        if not np.isfortran(new_array):
            new_array = np.asfortranarray(new_array)

        self.active = new_array

    def get_array(self, name=None):
        if name is None:
            name = self.active_name

        return self.arrays[name]

    @property
    def active(self):
        return self.arrays[self.active_name]

    @active.setter
    def active(self, array):
        self.arrays[self.active_name] = array


def _patch_utils():
    # Monkey patch tomviz.utils to support API outside Tomviz app.
    # I know this is a little yucky!
    def _get_scalars(dataobject, name=None):
        return dataobject.get_scalars(name)

    def _set_scalars(dataobject, new_scalars):
        dataobject.set_scalars(new_scalars)

    def _get_array(dataobject, name=None):
        return dataobject.get_array(name)

    def _set_array(dataobject, new_array):
        dataobject.set_array(new_array)

    def _set_tilt_angles(dataobject, tilt_angles):
        dataobject.tilt_angles = tilt_angles

    def _get_tilt_angles(dataobject):
        return dataobject.tilt_angles

    def _make_child_dataset(reference_dataset):
        child = np.empty_like(reference_dataset.active)
        arrays = {
            "ImageScalars": child
        }

        return DataObject(arrays)

    def _mark_as_volume(dataobject):
        dataobject.is_volume = True

    def _arrays(dataobject):
        return dataobject.arrays.items()

    utils.get_scalars = _get_scalars
    utils.set_scalars = _set_scalars
    utils.set_array = _set_array
    utils.get_array = _get_array
    utils.set_tilt_angles = _set_tilt_angles
    utils.get_tilt_angles = _get_tilt_angles
    utils.make_child_dataset = _make_child_dataset
    utils.mark_as_volume = _mark_as_volume
    utils.arrays = _arrays


def _execute_transform(operator_label, transform, arguments, input, progress):
    # Update the progress attribute to an instance that will work outside the
    # application and give use a nice progress information.
    spinner = None
    supports_progress = hasattr(transform, '__self__')
    if supports_progress:
        transform.__self__.progress = progress

        # Stub out the operator wrapper
        transform.__self__._operator_wrapper = OperatorWrapper()

    logger.info('Executing \'%s\' operator' % operator_label)
    if not supports_progress:
        print('Operator doesn\'t support progress updates.')

    result = None
    # Special case for SetTiltAngles
    if operator_label == 'SetTiltAngles':
        # Set the tilt angles so downstream operator can retrieve them
        # arguments the tilt angles.
        utils.set_tilt_angles(input, np.array(arguments['angles'],
                              dtype=np.float64))
    else:
        # Now run the operator
        result = transform(input, **arguments)
    if spinner is not None:
        update_spinner.cancel()
        spinner.finish()

    return result


def _load_transform_functions(operators):
    transform_functions = []
    for operator in operators:
        # Special case for tilt angles operator
        if operator['type'] == 'SetTiltAngles':
            # Just save the angles
            angles = {'angles': [float(a) for a in operator['angles']]}
            transform_functions.append((operator['type'], None, angles))
            continue

        if 'script' not in operator:
            raise Exception(
                'No script property found. C++ operator are not supported.')

        operator_script = operator['script']
        operator_label = operator['label']

        operator_module = _load_operator_module(operator_label, operator_script)
        transform = find_transform_function(operator_module)

        # partial apply the arguments
        arguments = {}
        if 'arguments' in operator:
            arguments = operator['arguments']

        transform_functions.append((operator_label, transform, arguments))

    return transform_functions


def _write_child_data(result, operator_index, output_file_path, dims):
    for (label, dataobject) in six.iteritems(result):
        # Only need write out data if the operator made updates.
        output_path = '.'
        if output_file_path is not None:
            output_path = os.path.dirname(output_file_path)

        # Make a directory with the operator index
        operator_path = os.path.join(output_path, str(operator_index))
        try:
            stat_result = os.stat(output_path)
            os.makedirs(operator_path)
            # We need to chown to the user and group who owns the output
            # path ( for docker execution )
            os.chown(operator_path, stat_result.st_uid, stat_result.st_gid)
        except OSError as exc:
            if exc.errno == errno.EEXIST and os.path.isdir(operator_path):
                pass
            else:
                raise

        # Now write out the data
        child_data_path = os.path.join(operator_path, '%s.emd' % label)
        _write_emd(child_data_path, dataobject, dims)


def execute(operators, start_at, data_file_path, output_file_path,
            progress_method, progress_path, read_options=None):

    if _is_data_exchange(data_file_path):
        output = _read_data_exchange(data_file_path, read_options)
    else:
        # Assume it is emd
        output = _read_emd(data_file_path, read_options)

    arrays = output['arrays']
    dims = output.get('dims')

    _patch_utils()

    # The first is the active array
    (active_array, _) = arrays[0]
    # Create dict of arrays
    arrays = {name: array for (name, array) in arrays}

    data = DataObject(arrays, active_array)
    if 'data_dark' in output:
        data.dark = output['data_dark']
    if 'data_white' in output:
        data.white = output['data_white']
    if 'tilt_angles' in output:
        data.tilt_angles = output['tilt_angles']
    if 'tilt_axis' in output:
        data.tilt_axis = output['tilt_axis']

    operators = operators[start_at:]
    transforms = _load_transform_functions(operators)
    with _progress(progress_method, progress_path) as progress:
        progress.started()
        operator_index = start_at
        for (label, transform, arguments) in transforms:
            progress.started(operator_index)
            result = _execute_transform(label, transform,
                                        arguments, data,
                                        progress)

            # Do we have any child data sources we need to write out?
            if result is not None:
                _write_child_data(result, operator_index,
                                  output_file_path, dims)

            progress.finished(operator_index)
            operator_index += 1

        logger.info('Execution complete.')
        # Now write out the transformed data.
        logger.info('Writing transformed data.')
        if output_file_path is None:
            output_file_path = '%s_transformed.emd' % \
                os.path.splitext(os.path.basename(data_file_path))[0]

        if result is None:
            _write_emd(output_file_path, data, dims)
        else:
            [(_, child_data)] = result.items()
            _write_emd(output_file_path, child_data, dims)
        logger.info('Write complete.')
        progress.finished()


if __name__ == '__main__':
    main()

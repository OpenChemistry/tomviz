import abc
import collections
import copy
import errno
import importlib
import json
import logging
import os
from pathlib import Path
import socket
import stat
import tempfile

import h5py
import numpy as np
from tqdm import tqdm

from tomviz._internal import find_transform_function

from tomviz.external_dataset import Dataset

LOG_FORMAT = '[%(asctime)s] %(levelname)s: %(message)s'

logger = logging.getLogger('tomviz')
logger.setLevel(logging.INFO)
stream_handler = logging.StreamHandler()
formatter = logging.Formatter(LOG_FORMAT)
stream_handler.setFormatter(formatter)
logger.addHandler(stream_handler)

DIMS = ['dim1', 'dim2', 'dim3']
ANGLE_UNITS = [b'[deg]', b'[rad]']

Dim = collections.namedtuple('Dim', 'path values name units')


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
    completed = False


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

    strides = options.get('subsampleSettings', {}).get('strides', [1] * 3)
    bnds = options.get('subsampleSettings', {}).get('volumeBounds', [-1] * 6)

    if len(bnds) != 6 or any(x < 0 for x in bnds):
        # Set the default bounds
        for i in range(3):
            bnds[i * 2] = 0
            bnds[i * 2 + 1] = dataset.shape[i]

    # These slice specifications are forwarded directly to the HDF5
    # "hyperslab" selections, and it loads only the data requested.
    return dataset[bnds[0]:bnds[1]:strides[0],
                   bnds[2]:bnds[3]:strides[1],
                   bnds[4]:bnds[5]:strides[2]]


def _swap_dims(dims, i, j):
    # Keeps the paths the same, but swaps everything else
    tmp = Dim(dims[i].path, dims[j].values, dims[j].name, dims[j].units)
    dims[j] = Dim(dims[j].path, dims[i].values, dims[i].name, dims[i].units)
    dims[i] = tmp


def _read_emd(path, options=None):
    with h5py.File(path, 'r') as f:
        tomography = f['data/tomography']

        dims = []
        for dim in DIMS:
            dims.append(Dim(dim,
                            tomography[dim][:],
                            tomography[dim].attrs['name'][0],
                            tomography[dim].attrs['units'][0]))

        data = tomography['data']
        # We default the name to ImageScalars
        name = data.attrs.get('name', 'ImageScalars')
        if isinstance(name, (np.ndarray, list, tuple)):
            name = name[0]

        if isinstance(name, (bytes, bytearray)):
            name = name.decode()

        arrays = [(name, _read_dataset(data, options))]

        # See if we have any additional channels
        tomviz_scalars = tomography.get('tomviz_scalars')
        if isinstance(tomviz_scalars, h5py.Group):
            # Only get hard linked datasets, since there is likely a
            # soft link pointing to "/data/tomography/data".
            def is_hard_link(name):
                link = tomviz_scalars.get(name, getlink=True)
                return isinstance(link, h5py.HardLink)

            keys = list(filter(is_hard_link, tomviz_scalars.keys()))
            # Get the datasets
            channel_datasets = [
                (key, _read_dataset(tomviz_scalars[key], options))
                for key in keys
            ]
            arrays += channel_datasets

        # If this is a tilt series, swap the X and Z axes
        tilt_axis = None
        if (
            dims[0].name in ('angles', b'angles') or
            dims[0].units in ANGLE_UNITS
        ):
            arrays = [(name, np.transpose(data, [2, 1, 0])) for (name, data)
                      in arrays]

            # Swap the first and last dims
            _swap_dims(dims, 0, -1)
            tilt_axis = 2

        # EMD stores data as row major order.  VTK expects column major order.
        arrays = [(name, np.asfortranarray(data)) for (name, data) in arrays]

        output = {
            'arrays': arrays,
            'dims': dims,
            'tilt_axis': tilt_axis,
            'metadata': {},
        }

        if dims is not None and dims[-1].name in ('angles', b'angles'):
            output['tilt_angles'] = dims[-1].values[:].astype(np.float64)

        return output


def _get_arrays_for_writing(dataset):
    tilt_angles = dataset.tilt_angles
    tilt_axis = dataset.tilt_axis
    active_array = dataset.active_scalars

    # Separate out the extra channels/arrays as we store them separately
    extra_arrays = {name: array for name, array
                    in dataset.arrays.items()
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

    # We can't do 16 bit floats, so up the size if they are 16 bit
    if active_array.dtype == np.float16:
        active_array = active_array.astype(np.float32)
    for key, value in extra_arrays.items():
        if value.dtype == np.float16:
            extra_arrays[key] = value.astype(np.float32)

    return active_array, extra_arrays


def _get_dims_for_writing(dataset, data, default_dims=None):
    tilt_angles = dataset.tilt_angles
    tilt_axis = dataset.tilt_axis
    spacing = dataset.spacing

    if default_dims is None:
        # Set the default dims
        dims = []
        names = ['x', 'y', 'z']

        for i, name in enumerate(names):
            values = np.array(range(data.shape[i]))
            dims.append(Dim('dim%d' % (i+1), values, name, '[n_m]'))
    else:
        # Make a deep copy to modify
        dims = copy.deepcopy(default_dims)

    # Override the dims if spacing is set
    if spacing is not None:
        for i, dim in enumerate(dims):
            end = data.shape[i] * spacing[i]
            res = np.arange(0, end, spacing[i])
            dims[i] = Dim(dim.path, res, dim.name, dim.units)

    if tilt_angles is not None:
        if tilt_axis == 2:
            # Swap the first and last dims
            _swap_dims(dims, 0, -1)

        units = dims[0].units if dims[0].units in ANGLE_UNITS else '[deg]'

        # Make the x axis the tilt angles
        dims[0] = Dim(dims[0].path, tilt_angles, 'angles', units)
    else:
        # In case the input was a tilt series, make the first dim x,
        # and the units [n_m]
        if dims[0].name in ('angles', b'angles'):
            dims[0].name = 'x'
            dims[0].units = '[n_m]'

    return dims


def _write_emd(path, dataset, dims=None):
    active_array, extra_arrays = _get_arrays_for_writing(dataset)

    with h5py.File(path, 'w') as f:
        f.attrs.create('version_major', 0, dtype='uint32')
        f.attrs.create('version_minor', 2, dtype='uint32')
        data_group = f.create_group('data')
        tomography_group = data_group.create_group('tomography')
        tomography_group.attrs.create('emd_group_type', 1, dtype='uint32')
        data = tomography_group.create_dataset('data', data=active_array)
        data.attrs['name'] = np.bytes_(dataset.active_name)

        dims = _get_dims_for_writing(dataset, data, dims)

        # add dimension vectors
        for dim in dims:
            d = tomography_group.create_dataset(dim.path, dim.values.shape)
            d.attrs['name'] = np.bytes_(dim.name)
            d.attrs['units'] = np.bytes_(dim.units)
            d[:] = dim.values

        # If we have extra scalars add them under tomviz_scalars
        tomviz_scalars = tomography_group.create_group('tomviz_scalars')
        if extra_arrays:
            for (name, array) in extra_arrays.items():
                tomviz_scalars.create_dataset(name, data=array)

        # Create a soft link to the active array
        active_name = dataset.active_name
        tomviz_scalars[active_name] = h5py.SoftLink('/data/tomography/data')


def _read_data_exchange(path: Path, options: dict | None = None):
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
            datasets = {name: np.asfortranarray(data)
                        for (name, data) in datasets.items()}

            if 'theta' in datasets:
                # Swap x and z axes
                swap_keys = ['data', 'data_dark', 'data_white']
                for key in swap_keys:
                    if key in datasets:
                        datasets[key] = np.transpose(datasets[key], [2, 1, 0])

        tilt_axis = None
        if 'theta' in datasets:
            tilt_axis = 2 if to_fortran else 0

        output = {
            'arrays': [(path.stem, datasets.get('data'))],
            'data_dark': datasets.get('data_dark'),
            'data_white': datasets.get('data_white'),
            'tilt_angles': datasets.get('theta'),
            'tilt_axis': tilt_axis,
            'metadata': {},
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


def _execute_transform(operator_label, transform, arguments, input, progress,
                       dataset_dependencies):
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

    # Convert any data source id arguments here into Dataset objects.
    for k, v in arguments.items():
        if isinstance(v, str) and v.startswith('0x'):
            arguments[k] = dataset_dependencies[v]

    result = None
    # Special cases for marking as volume or tilt series
    if operator_label == 'ConvertToVolume':
        # Easy peasy
        input.tilt_angles = None
    elif operator_label == 'SetTiltAngles':
        # Set the tilt angles so downstream operator can retrieve them
        # arguments the tilt angles.
        input.tilt_angles = np.array(arguments['angles']).astype(np.float64)
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
        # Special case for marking as volume or tilt series
        if operator['type'] == 'ConvertToVolume':
            transform_functions.append((operator['type'], None, {}))
            continue
        elif operator['type'] == 'SetTiltAngles':
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
    for label, dataobject in result.items():
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


def load_dataset(data_file_path, read_options=None):
    data_file_path = Path(data_file_path)
    if _is_data_exchange(data_file_path):
        output = _read_data_exchange(data_file_path, read_options)
    else:
        # Assume it is emd
        output = _read_emd(data_file_path, read_options)

    arrays = output['arrays']
    dims = output.get('dims')
    metadata = output.get('metadata', {})

    # The first is the active array
    (active_array, _) = arrays[0]
    # Create dict of arrays
    arrays = {name: array for (name, array) in arrays}

    data = Dataset(arrays, active_array)
    data.file_name = os.path.abspath(data_file_path)
    data.metadata = metadata

    if 'data_dark' in output:
        data.dark = output['data_dark']
    if 'data_white' in output:
        data.white = output['data_white']
    if 'tilt_angles' in output:
        data.tilt_angles = output['tilt_angles']
    if 'tilt_axis' in output:
        data.tilt_axis = output['tilt_axis']
    if dims is not None:
        # Convert to native type, as is required by itk
        data.spacing = [float(d.values[1] - d.values[0]) for d in dims]

    data.dims = dims

    return data


def run_pipeline(data, operators, start_at, progress, child_output_path=None,
                 dataset_dependencies=None):

    dims = data.dims
    operators = operators[start_at:]
    transforms = _load_transform_functions(operators)

    operator_index = start_at
    for (label, transform, arguments) in transforms:
        progress.started(operator_index)
        result = _execute_transform(label, transform, arguments, data,
                                    progress, dataset_dependencies)

        # Do we have any child data sources we need to write out?
        if child_output_path and result is not None:
            _write_child_data(result, operator_index, child_output_path, dims)

        progress.finished(operator_index)
        operator_index += 1

        logger.info('Execution complete.')

    return result


def execute(operators, start_at, data_file_path, output_file_path,
            progress_method, progress_path, read_options=None,
            dataset_dependencies=None):

    if dataset_dependencies is None:
        dataset_dependencies = {}

    child_output_path = output_file_path
    if child_output_path is None:
        child_output_path = '.'

    data = load_dataset(data_file_path, read_options)
    dims = data.dims

    with _progress(progress_method, progress_path) as progress:
        progress.started()

        # Run the pipeline
        result = run_pipeline(data, operators, start_at, progress,
                              child_output_path, dataset_dependencies)

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

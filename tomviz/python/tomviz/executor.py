import h5py
import importlib
import os
import numpy
import logging
import tempfile
import sys
import socket
import abc
import stat
import json
from contextlib import contextmanager
import six
import abc

from tqdm import tqdm

from tomviz import utils
from tomviz._internal import find_transform_scalars
from tomviz.py2to3 import py3

LOG_FORMAT = '[%(asctime)s] %(levelname)s: %(message)s'

logger = logging.getLogger('tomviz')
logger.setLevel(logging.INFO)
stream_handler = logging.StreamHandler()
formatter = logging.Formatter(LOG_FORMAT)
stream_handler.setFormatter(formatter)
logger.addHandler(stream_handler)

DIMS = ['dim1', 'dim2', 'dim3']

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

@six.add_metaclass(abc.ABCMeta)
class JsonProgress(ProgressBase):
    """
    Abstract class used to update operator progress using JSON based messages.
    """

    @abc.abstractmethod
    def write(self, data):
        """
        Method the write JSON progress message. Implemented by subclass.
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
            'messages': msg
        }
        self.write(m)

        self._message = msg

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

class LocalSocketProgress(JsonProgress):
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

class FilesProgress(JsonProgress):
    """
    Class used to update operator progress by writing a sequence of files to a
    directory.
    """
    def __init__(self, path):
        self._path = path
        self._sequence_number = 0

    def write(self, data):
        file_path = os.path.join(self._path, 'progress%d' % self._sequence_number)
        self._sequence_number += 1

        with open(file_path, 'w') as f:
            json.dump(data, f)

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
        if py3:
            spec = importlib.util.spec_from_file_location(label, path)
            operator_module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(operator_module)
        else:
            module_dir = os.path.dirname(path)
            sys.path.append(module_dir)
            operator_module = importlib.import_module(
                os.path.splitext(
                    os.path.basename(path))[0])
            sys.path.remove(module_dir)
    finally:
        if fd is not None:
            os.close(fd)
        if path is not None:
            os.unlink(path)

    return operator_module


def _read_emd(path):
    with h5py.File(path, 'r') as f:
        # assuming you know the structure of the file
        tomography = f['data/tomography']

        dims = []
        for dim in DIMS:
            dims.append((dim,
                         tomography[dim][:],
                         tomography[dim].attrs['name'][0],
                         tomography[dim].attrs['name'][0]))

        return (tomography['data'][:], dims)


def _write_emd(path, data, dims):
    with h5py.File(path, 'w') as f:
        f.attrs.create('version_major', 0, dtype='uint32')
        f.attrs.create('version_minor', 2, dtype='uint32')
        data_group = f.create_group('data')
        tomography_group = data_group.create_group('tomography')
        tomography_group.attrs.create('emd_group_type', 1, dtype='uint32')
        tomography_group.create_dataset('data', data=data)

        # add dimension vectors
        for (dataset_name, value, name, units) in dims:
            d = tomography_group.create_dataset(dataset_name, (2,))
            d.attrs['name'] = numpy.string_(name)
            d.attrs['units'] = numpy.string_(units)
            d[:] = value


def _execute_transform(operator_label, transform, arguments, input, progress):
    # Monkey patch tomviz.utils to make get_scalars a no-op and allow use to
    # retrieve the transformed data from set_scalars. I know this is a little
    # yucky! And yes I know this is not thread safe!
    utils.get_scalars = lambda d: d
    utils.get_array = lambda d: d
    # Container to allow use to capture the transformed data, in Python 3 we
    # could use nonlocal :-)
    transformed_scalars_container = {}

    def _set_scalars(dataobject, new_scalars):
        transformed_scalars_container['data'] = new_scalars

    def _set_array(dataobject, new_scalars):
        transformed_scalars_container['data'] = new_scalars

    utils.set_scalars = _set_scalars
    utils.set_array = _set_array

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
    # Now run the operator
    transform(input, **arguments)
    if spinner is not None:
        update_spinner.cancel()
        spinner.finish()

    logger.info('Execution complete.')

    return transformed_scalars_container['data']


def _load_transform_functions(operators):
    transform_functions = []
    for operator in operators:

        if 'script' not in operator:
            raise Exception(
                'No script property found. C++ operator are not supported.')

        operator_script = operator['script']
        operator_label = operator['label']

        operator_module = _load_operator_module(operator_label, operator_script)
        transform_scalars = find_transform_scalars(operator_module)

        # partial apply the arguments
        arguments = {}
        if 'arguments' in operator:
            arguments = operator['arguments']

        transform_functions.append((operator_label, transform_scalars,
                                    arguments))

    return transform_functions


def execute(operators, start_at, data_file_path, output_file_path,
            progress_method, progress_path):
    data, dims = _read_emd(data_file_path)

    operators = operators[start_at:]
    transforms = _load_transform_functions(operators)
    with _progress(progress_method, progress_path) as progress:
        progress.started()
        operator_index = start_at
        for (label, transform, arguments) in transforms:
            progress.started(operator_index)
            data = _execute_transform(label, transform, arguments, data, progress)
            progress.finished(operator_index)
            operator_index +=1

        # Now write out the transformed data.
        logger.info('Writing transformed data.')
        if output_file_path is None:
            output_file_path = '%s_transformed.emd' % \
                os.path.splitext(os.path.basename(data_file_path))[0]
        _write_emd(output_file_path, data, dims)
        logger.info('Write complete.')
        progress.finished()



if __name__ == '__main__':
    main()

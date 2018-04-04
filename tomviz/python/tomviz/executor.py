import h5py
import importlib
import os
import numpy
import logging
import tempfile
import sys
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


class Progress(object):
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


def execute(operator, data_file_path, output_file_path):
    operator_script = operator['script']
    operator_label = operator['label']

    data, dims = _read_emd(data_file_path)

    operator_module = _load_operator_module(operator_label, operator_script)
    transform_scalars = find_transform_scalars(operator_module)

    # Monkey patch tomviz.utils to make get_scalars a no-op and allow use to
    # retrieve the transformed data from set_scalars. I know this is a little
    #yucky! And yes I know this is not thread safe!
    utils.get_scalars = lambda d: d
    # Container to allow use to capture the transformed data, in Python 3 we
    # could use nonlocal :-)
    transformed_scalars_container = {}

    def _set_scalars(dataobject, new_scalars):
        transformed_scalars_container['data'] = new_scalars

    utils.set_scalars = _set_scalars

    # Update the progress attribute to an instance that will work outside the
    # application and give use a nice progress bar
    with Progress() as progress:
        transform_scalars.__self__.progress = progress

        # Stub out the operator wrapper
        transform_scalars.__self__._operator_wrapper = OperatorWrapper()

        logger.info('Executing \'%s\' operator' % operator_label)
        # Now run the operator
        transform_scalars(data)
        logger.info('Execution complete.')

    # Now write out the transformed data.
    logger.info('Writing transformed data.')
    if output_file_path is None:
        output_file_path = '%s_transformed.emd' % \
            os.path.splitext(os.path.basename(data_file_path))[0]
    _write_emd(output_file_path, transformed_scalars_container['data'], dims)
    logger.info('Write complete.')


if __name__ == '__main__':
    main()

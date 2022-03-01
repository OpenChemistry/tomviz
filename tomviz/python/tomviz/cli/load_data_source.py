import logging
from pathlib import Path

from tomviz import executor

logger = logging.getLogger('tomviz')


def create_read_options(data_source):
    read_options = {}
    if 'subsampleSettings' in data_source:
        key = 'subsampleSettings'
        read_options[key] = data_source[key].copy()
    if 'keepCOrdering' in data_source:
        read_options['keep_c_ordering'] = data_source['keepCOrdering']

    return read_options


def extract_data_path(data_source, state_file_path):
    if 'reader' not in data_source:
        raise Exception('Data source does not contain a reader.')
    filenames = data_source['reader']['fileNames']
    if len(filenames) > 1:
        raise Exception('Image stacks not supported.')
    data_path = filenames[0]
    # fileName is relative to the state file location, so convert to
    # absolute path.
    data_path = (Path(state_file_path).parent / data_path).resolve()
    if not data_path.exists():
        raise Exception('Data source path does not exist: %s'
                        % data_path)

    return data_path


def load_data_source(data_source, state_file_path):
    read_options = create_read_options(data_source)
    data_path = extract_data_path(data_source, state_file_path)

    logger.info(f'Loading data at: {data_path}')
    return executor.load_dataset(data_path, read_options)

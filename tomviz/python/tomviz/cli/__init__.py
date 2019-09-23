import click
import json
import os
import logging
from pathlib import Path

from tomviz import executor

logger = logging.getLogger('tomviz')


def _extract_pipeline(state):
    if 'dataSources' not in state:
        raise Exception('Invalid state file: \'dataSources\' not found.')

    data_sources = state['dataSources']

    if len(data_sources) > 1:
        raise Exception(
            'Only state files with a single data source are supported.')

    if len(data_sources) != 1:
        raise Exception('No data source found.')

    data_source = data_sources[0]

    if 'operators'not in data_source:
        raise Exception('\'operators\' not found.')

    operators = data_source['operators']

    if len(operators) == 0:
        raise Exception('No operators found.')

    return (data_source, operators)


@click.command(name="tomviz")
@click.option('-d', '--data-path', help='Path to an EMD/Data Exchange file or'
              ' directory containing EMD/Data Exchange files, can be used'
              ' to override data source in state file. If multiple files are'
              ' provided the pipeline with be run for each file.',
              type=click.Path(exists=True))
@click.option('-s', '--state-file-path', help='Path to the Tomviz state file',
              type=click.Path(exists=True), required=True)
@click.option('-o', '--output-file-path',
              help='Path to write the transformed dataset.', type=click.Path())
@click.option('-p', '--progress-method',
              help='The method to use to progress updates.',
              type=click.Choice(['tqdm', 'socket', 'files']), default='tqdm')
@click.option('-u', '--socket-path',
              help='The socket path to use for progress updates.',
              type=click.Path(), default='/tomviz/progress')
@click.option('-i', '--operator-index',
              help='The operator to start at.',
              type=int, default=0)
def main(data_path, state_file_path, output_file_path, progress_method,
         socket_path, operator_index):

    # Extract the pipeline
    with open(state_file_path) as fp:
        state = json.load(fp, encoding='utf-8')

    (datasource, operators) = _extract_pipeline(state)

    read_options = {}
    if 'subsampleSettings' in datasource:
        key = 'subsampleSettings'
        read_options[key] = datasource[key].copy()
    if 'keepCOrdering' in datasource:
        read_options['keep_c_ordering'] = datasource['keepCOrdering']

    # if we have been provided a data file path we are going to use the one
    # from the state file, so check it exists.
    if data_path is None:
        if 'reader' not in datasource:
            raise Exception('Data source does not contain a reader.')
        filenames = datasource['reader']['fileNames']
        if len(filenames) > 1:
            raise Exception('Image stacks not supported.')
        data_path = filenames[0]
        # fileName is relative to the state file location, so convert to
        # absolute path.
        data_path = os.path.abspath(
            os.path.join(os.path.dirname(state_file_path), data_path))
        if not os.path.exists(data_path):
            raise Exception('Data source path does not exist: %s'
                            % data_path)

    exts = ['.emd', '.h5', '.hdf5']
    data_path = Path(data_path)
    # Do we have multiple files to operate on
    if data_path.is_dir():
        data_file_paths = []
        for ext in exts:
            data_file_paths += list(data_path.glob('*' + ext))

        output_file_paths \
            = ['%s_transformed.emd' % x.stem for x in data_file_paths]
        # If we have been give an output directory write the output there
        if output_file_path is not None and Path(output_file_path).is_dir():
            output_file_paths \
                = [Path(output_file_path) / x for x in output_file_paths]
    else:
        if data_path.suffix.lower() not in exts:
            raise Exception(
                'Unsupported data source format, only HDF5 formats supported.')
        data_file_paths = [data_path]
        output_file_paths = [output_file_path]

    number_of_files = len(data_file_paths)
    if number_of_files == 0:
        logger.warning('No data files found.')
    elif number_of_files > 1:
        logger.info('Executing pipeline on %d files.' % number_of_files)

    for (data_file_path, output_file_path) in zip(data_file_paths,
                                                  output_file_paths):
        logger.info('Executing pipeline on %s' % data_file_path)
        executor.execute(operators, operator_index, data_file_path,
                         output_file_path, progress_method, socket_path,
                         read_options)

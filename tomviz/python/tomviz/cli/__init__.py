import click
import json
import logging
from pathlib import Path

from tomviz import executor

from .load_data_source import create_read_options, extract_data_path
from .data_source_dependencies import load_dependencies

logger = logging.getLogger('tomviz')


def _create_data_source_options(data_sources):
    ids = [x['id'] for x in data_sources]
    filenames = [x['reader']['fileNames'][0] for x in data_sources]
    return dict(zip(ids, filenames))


def _data_source_options_string(options):
    s = ''
    for k, v in options.items():
        s += f'{k}  ({v})\n\n'

    return s


def _create_data_source_options_string(data_sources):
    options = _create_data_source_options(data_sources)
    return _data_source_options_string(options)


def _auto_select_data_source(data_sources):
    if len(data_sources) == 1:
        # Only one data source. Select this one.
        if 'id' not in data_sources[0]:
            # For backwards compatibility, when we did not have data
            # source ids (as is being done in the CI), make an id.
            data_sources[0]['id'] = '0x0'
        return data_sources[0]['id']

    with_pipeline = [len(x.get('operators', [])) > 0 for x in data_sources]
    if sum(with_pipeline) == 0:
        raise Exception('No operators were found')

    if sum(with_pipeline) > 1:
        options = _create_data_source_options_string(data_sources)
        msg = (
            'Multiple data sources with operators were found. One must be '
            'selected explicitly with the `-x` argument so we know which '
            f'pipeline to use. Available options:\n\n{options}'
        )
        raise Exception(msg)

    return data_sources[with_pipeline.index(True)]['id']


def _extract_pipeline(state, selected_data_source=None):
    if 'dataSources' not in state:
        raise Exception('Invalid state file: \'dataSources\' not found.')

    data_sources = state['dataSources']

    if not data_sources:
        raise Exception('No data source found.')

    if selected_data_source is None:
        selected_data_source = _auto_select_data_source(data_sources)

    ids = [x['id'] for x in data_sources]
    if selected_data_source not in ids:
        options = _create_data_source_options_string(data_sources)
        msg = (
            f'{selected_data_source} is not in the list of data sources! '
            f'Available options:\n\n{options}'
        )
        raise Exception(msg)

    data_source = data_sources[ids.index(selected_data_source)]

    if 'operators'not in data_source:
        raise Exception('\'operators\' not found.')

    operators = data_source['operators']

    if len(operators) == 0:
        raise Exception('No operators found.')

    return data_source, operators


@click.command(name="tomviz")
@click.option('-d', '--data-path', help='Path to an EMD/Data Exchange file or'
              ' directory containing EMD/Data Exchange files, can be used'
              ' to override data source in state file. If multiple files are'
              ' provided the pipeline with be run for each file.',
              type=click.Path(exists=True))
@click.option('-s', '--state-file-path', help='Path to the Tomviz state file',
              type=click.Path(exists=True), required=True)
@click.option('-x', '--selected-data-source',
              help='Explicitly select the id of the data source from which '
                   'the pipeline will be used',
              type=str, default=None)
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
         socket_path, operator_index, selected_data_source):

    # Extract the pipeline
    with open(state_file_path, encoding='utf-8') as fp:
        state = json.load(fp)

    data_source, operators = _extract_pipeline(state, selected_data_source)

    # Load the dependencies for the data source
    dependencies = load_dependencies(data_source, state, state_file_path,
                                     progress_method, socket_path)

    # Create the read options
    read_options = create_read_options(data_source)

    # if we have been provided a data file path we are going to use the one
    # from the state file, so check it exists.
    if data_path is None:
        data_path = extract_data_path(data_source, state_file_path)

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
                         read_options, dependencies)

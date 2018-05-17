import click
import json
import os

from tomviz import executor


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
@click.option('-d', '--data-file-path', help='Path to the EMD file, can be used'
              ' to override data source in state file',
              type=click.Path(exists=True))
@click.option('-s', '--state-file-path', help='Path to the Tomviz state file',
              type=click.Path(exists=True), required=True)
@click.option('-o', '--output-file-path',
              help='Path to write the transformed dataset.', type=click.Path())
@click.option('-p', '--progress-method',
              help='The method to use to progress updates.',
              type=click.Choice(['tqdm', 'socket']), default='tqdm')
@click.option('-u', '--socket-path',
              help='The socket path to use for progress updates.',
              type=click.Path(), default='/tomviz/progress')
def main(data_file_path, state_file_path, output_file_path, progress_method,
         socket_path):

    # Extract the pipeline
    with open(state_file_path, 'rb') as fp:
        state = json.load(fp)

    (datasource, operators) = _extract_pipeline(state)

    # if we have been provided a data file path we are going to use the one
    # from the state file, so check it exists.
    if data_file_path is None:
        if 'reader' not in datasource:
            raise Exception('Data source does not contain a reader.')
        filenames = datasource['reader']['fileNames']
        if len(filenames) > 1:
            raise Exception('Image stacks not supported.')
        data_file_path = filenames[0]
        # fileName is relative to the state file location, so convert to
        # absolute path.
        data_file_path = os.path.abspath(
            os.path.join(os.path.dirname(state_file_path), data_file_path))
        if not data_file_path.lower().endswith('.emd'):
            raise Exception(
                'Unsupported data source format, only EMD is supported.')
        if not os.path.exists(data_file_path):
            raise Exception('Data source path does not exist: %s'
                            % data_file_path)

    executor.execute(operators, data_file_path, output_file_path,
                     progress_method, socket_path)

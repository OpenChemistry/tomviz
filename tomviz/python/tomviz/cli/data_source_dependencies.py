import copy
import logging

from tomviz import executor

from .load_data_source import load_data_source

logger = logging.getLogger('tomviz')


def data_source_dependencies(data_source):
    deps = []
    for op in data_source.get('operators', []):
        for value in op.get('arguments', {}).values():
            if isinstance(value, str) and value.startswith('0x'):
                deps.append(value)

        for child in op.get('dataSources', []):
            deps += data_source_dependencies(child)

    return deps


class DependencyInfo:
    def __init__(self, dep, data_sources):
        self.dep = dep
        self.data_sources = data_sources
        self._path = None

    @property
    def path(self):
        # After it has been computed once, cache the path.
        if self._path is not None:
            return self._path

        path = []

        def recurse(data_sources):
            for ds in data_sources:
                if ds.get('id', '') == self.dep:
                    path.insert(0, ds)
                    return True

                for op in ds.get('operators', []):
                    children = op.get('dataSources', [])
                    if recurse(children):
                        path.insert(0, ds)
                        return True

        recurse(self.data_sources)
        self._path = path
        return path

    @property
    def root_data_source(self):
        return self.path[0]

    @property
    def depth(self):
        return len(self.path) - 1

    @property
    def is_root(self):
        return self.depth == 0

    @property
    def is_child(self):
        return self.depth > 0


def load_dependencies(target_source, state, state_file_path, progress_method,
                      progress_path):
    already_loaded = {}
    data_sources = state.get('dataSources', [])

    def local_load_data_source(data_source):
        id = data_source.get('id', '')
        if id not in already_loaded:
            already_loaded[id] = load_data_source(data_source, state_file_path)

        return already_loaded[id]

    def local_load_dependencies(data_source):
        deps = data_source_dependencies(data_source)
        if not deps:
            # Nothing to do
            return already_loaded

        for dep in deps:
            # Load the root data source of this dependency
            info = DependencyInfo(dep, data_sources)
            root_source = info.root_data_source

            # First, load in the root source
            root_dataset = local_load_data_source(root_source)

            # Load any dependencies of this data source
            local_load_dependencies(root_source)

            # If it is a child dependency, execute the pipeline
            if info.is_child:
                # Make sure it has a depth of 1. Otherwise, raise an
                # exception, as we only support a depth of 1.
                if info.depth != 1:
                    msg = (
                        f'Dependency {dep} has a depth of {info.depth}, '
                        'but only a depth of 0 or 1 is currently supported '
                    )
                    raise Exception(msg)

                # Execute the pipeline. The result is what we need.
                with executor._progress(progress_method,
                                        progress_path) as progress:
                    progress.started()
                    logger.info(
                        f'Running pipeline to create child dependency: {dep}')

                    # The operator might modify the dataset in place.
                    # We also might still need the root dataset as it is, so
                    # make a deep copy.
                    data = copy.deepcopy(root_dataset)
                    kwargs = {
                        'data': data,
                        'operators': root_source['operators'],
                        'start_at': 0,
                        'progress': progress,
                        'child_output_path': None,
                        'dataset_dependencies': already_loaded,
                    }

                    result = executor.run_pipeline(**kwargs)
                    if result is None:
                        # The data was modified in place
                        result = data

                    already_loaded[dep] = result

                    logger.info(f'Child dependency created: {dep}')
                    progress.finished()

        return already_loaded

    return local_load_dependencies(target_source)

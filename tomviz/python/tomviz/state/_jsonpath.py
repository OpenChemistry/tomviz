from jsonpointer import resolve_pointer


def find_pipeline(path):
    from . import pipelines

    if len(path) < 2:
        raise ValueError("Path doesn't have enough parts.")

    obj_type = path.pop(0)
    if obj_type != 'dataSources':
        raise ValueError("Path doesn't start with 'dataSources'.")

    pipeline_index = int(path.pop(0))

    if pipeline_index >= len(pipelines):
        raise ValueError('Pipeline index no longer exists.')

    return pipelines[pipeline_index]


def find_operator(path):
    pipeline = find_pipeline(path)

    if len(path) < 1:
        raise ValueError("Path doesn't have enough parts.")

    obj_type = path.pop(0)
    if obj_type != "operators":
        raise ValueError("Path doesn't contain 'operators'.")

    op_index = -1
    if path:
        op_index = path.pop(0)
        op_index = int(op_index)

    operators = pipeline.dataSource.operators
    if op_index >= len(operators):
        raise ValueError("Operator index no longer exists.")

    return operators[op_index]


def find_datasource(path):
    copy = path.copy()
    pipeline = find_pipeline(copy)

    # If the path has no other dataSources we are done
    if 'dataSources' not in copy:
        del path[:2]

        return pipeline.dataSource

    # Find the operator (the child data source must be associated with one ...)
    op = find_operator(path)
    del path[:2]

    return op.dataSources[0]


def find_module(path):
    # First find the data source the module is attached to.
    datasource = find_datasource(path)
    if datasource is None:
        raise ValueError('Unable to find data source.')

    if len(path) < 1:
        raise ValueError("Path doesn't have enough parts.")

    obj_type = path.pop(0)
    if obj_type != 'modules':
        raise ValueError("Path doesn't contain 'modules'.")

    mod_index = -1
    if path:
        mod_index = path.pop(0)
        mod_index = int(mod_index)

    modules = datasource.modules
    if mod_index >= len(modules):
        raise ValueError("Module index no longer exists.")

    return modules[mod_index]


# Note: This can only use using during a _update_state(...) call!
def find_removed_object_id(path):
    # _state is the old state containing the remove object
    from . import _state
    obj = resolve_pointer(_state, path)

    return obj['id']


def _path(path, obj_type):
    path = path.split('/')
    # Find last occurrence
    index = len(path) - 1 - path[::-1].index(obj_type)
    path = path[0:index+2]

    return '/'.join(path)


def operator_path(path):
    return _path(path, 'operators')


def module_path(path):
    return _path(path, 'modules')


def datasource_path(path):
    return _path(path, 'dataSources')


def pipeline_index(path):
    return int(path.split('/')[2])

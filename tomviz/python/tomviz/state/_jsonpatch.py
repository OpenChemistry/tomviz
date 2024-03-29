import jsonpatch
import re
import json

from jsonpointer import resolve_pointer

from ._jsonpath import (
    operator_path,
    module_path,
    datasource_path,
    pipeline_index,
    find_operator,
    find_module,
    find_datasource,
    find_removed_object_id

)

from ._schemata import (
    load_operator,
    load_module,
    load_datasource,
    dump_module,
    dump_operator,
    dump_datasource
)

from ._models import Pipeline

from ._pipeline import PipelineStateManager


def update_list(src, dst):
    same = len(src) == len(dst)
    if len(src) == len(dst):
        for (x, y) in zip(src, dst):
            same = hasattr(x, 'id') and hasattr(y, 'id') and x.id == y.id
            if not same:
                break

    # If we are dealing with a list of the same object we can just update
    if same:
        for i in range(0, len(src)):
            x = src[i]
            if isinstance(x, built_in):
                dst[i] = x
            else:
                update(x, dst[i])
    # Otherwise, clear the list and start again
    else:
        for x in dst:
            if hasattr(x, '_kill'):
                x._kill()
        dst.clear()
        for x in src:
            dst.append(x)


built_in = (str, int, float, complex, dict)


def update(src, dst):
    for attr, value in vars(src).items():
        if isinstance(value, list):
            if not hasattr(dst, attr):
                setattr(dst, attr, value)
            else:
                update_list(value, getattr(dst, attr))
        elif isinstance(value, built_in):
            setattr(dst, attr, value)
        else:
            if not hasattr(dst, attr) or getattr(dst, attr) is None:
                setattr(dst, attr, value)
            else:
                update(value, getattr(dst, attr))

    return dst


def operator_update(patch_op):
    # First get path to operator
    path = patch_op['path']
    op_path = operator_path(path)

    # Now get the current state of the operator
    op = find_operator(op_path.split('/')[1:])
    current_state = PipelineStateManager().serialize_op(path, op.id)
    current_state_dct = json.loads(current_state)

    # Apply the update
    # Adjust the path
    patch_op['path'] = path.replace(op_path, '')
    patch = jsonpatch.JsonPatch([patch_op])
    current_state_dct = patch.apply(current_state_dct)

    # Now update the operator
    PipelineStateManager().update_op(op_path, json.dumps(current_state_dct))

    return op_path


def module_update(patch_module):
    # First get path to module
    path = patch_module['path']
    mod_path = module_path(path)

    # Now get the current state of the module
    module = find_module(mod_path.split('/')[1:])
    current_state = PipelineStateManager().serialize_module(path, module.id)
    current_state_dct = json.loads(current_state)

    # Apply the update
    # Adjust the path
    patch_module['path'] = path.replace(mod_path, '')
    patch = jsonpatch.JsonPatch([patch_module])
    current_state_dct = patch.apply(current_state_dct)

    # Now update the module
    PipelineStateManager().update_module(mod_path,
                                         json.dumps(current_state_dct))

    return mod_path


def datasource_update(patch_datasource):
    # First get path to datasource
    path = patch_datasource['path']
    ds_path = datasource_path(path)

    # Now get the current state of the datasource
    datasource = find_datasource(ds_path.split('/')[1:])
    current_state = PipelineStateManager().serialize_datasource(path,
                                                                datasource.id)
    current_state_dct = json.loads(current_state)

    # Apply the update
    # Adjust the path
    patch_datasource['path'] = path.replace(ds_path, '')
    patch = jsonpatch.JsonPatch([patch_datasource])
    current_state_dct = patch.apply(current_state_dct)

    # Now update the datasource
    PipelineStateManager().update_datasource(ds_path,
                                             json.dumps(current_state_dct))

    return ds_path


def module_add(patch_module):
    if patch_module['value'] == []:
        return

    module_json = patch_module['value']
    if isinstance(module_json, list):
        module_json = module_json[0]
    path = patch_module['path']
    ds_path = datasource_path(path)
    data_source = find_datasource(ds_path.split('/')[1:])

    state = PipelineStateManager().add_module(ds_path, data_source.id,
                                              module_json['type'])
    # If we are adding our first module just find the datasource and the first
    #  module
    parts = path.split('/')[1:]
    if path.endswith('modules'):
        module = find_datasource(parts).modules[0]
        path += '/0'
    else:
        module = find_module(parts)

    module.properties.visibility = True
    updates = module._updates()
    state = jsonpatch.JsonPatch(updates).apply(json.loads(state))

    # Now update the module
    PipelineStateManager().update_module(path, json.dumps(state))

    module_state = PipelineStateManager().serialize_module(path, state['id'])
    new_module = load_module(json.loads(module_state))
    update(new_module, module)


def operator_add(patch_op):
    if patch_op['value'] == []:
        return

    op_json = patch_op['value']

    path = patch_op['path']
    ds_path = datasource_path(path)
    data_source = find_datasource(ds_path.split('/')[1:])

    for o in op_json if isinstance(op_json, list) else [op_json]:
        op_state = PipelineStateManager().add_operator(ds_path, data_source.id,
                                                       json.dumps(o))

        op = find_operator(path.split('/')[1:])
        new_op = load_operator(json.loads(op_state))
        update(new_op, op)


def datasource_add(patch_datasource):
    if patch_datasource['value'] == []:
        return

    datasource_json = patch_datasource['value']
    # if isinstance(datasource_json, list):
    #     datasource_json = datasource_json[0]
    path = patch_datasource['path']

    datasource = find_datasource(path.split('/')[1:])
    datasource_state = \
        PipelineStateManager().add_datasource(json.dumps(datasource_json))
    new_datasource = load_datasource(json.loads(datasource_state))
    update(new_datasource, datasource)


def operator_remove(patch_op):
    path = patch_op['path']
    ds_path = datasource_path(path)
    data_source = find_datasource(ds_path.split('/')[1:])

    op_id = ''
    # Are we removing them all?
    if not path.endswith('operators'):
        op_id = find_removed_object_id(path)

    PipelineStateManager().remove_operator(path, data_source.id, op_id)


def module_remove(patch_module):
    path = patch_module['path']
    ds_path = datasource_path(path)
    data_source = find_datasource(ds_path.split('/')[1:])

    module_id = ''
    # Are we removing them all?
    if not path.endswith('modules'):
        module_id = find_removed_object_id(path)

    PipelineStateManager().remove_module(path, data_source.id, module_id)


def datasource_remove(patch_datasource):
    path = patch_datasource['path']

    ds_id = ''
    # Are we removing them all?
    if not path.endswith('dataSources'):
        ds_id = find_removed_object_id(path)

    PipelineStateManager().remove_datasource(path, ds_id)


_datasource_regx = re.compile(r'.*/dataSources/\d(.*)')


def is_datasource_update(path):
    datasource_match = _datasource_regx.match(path)

    return (datasource_match and
            (is_module_update(datasource_match.group(1)) is None) and
            (is_operator_update(datasource_match.group(1)) is None))


_module_regx = re.compile(r'.*/modules/\d(.*)')


def is_module_update(path):
    module_match = _module_regx.match(path)

    return (module_match and
            (is_datasource_update(module_match.group(1)) is None))


_op_regx = re.compile(r'.*/operators/\d(.*)')


def is_operator_update(path):
    op_match = _op_regx.match(path)

    return op_match and (is_datasource_update(op_match.group(1)) is None)


_module_add_regx = re.compile(r'.*/modules/?\d*$')


def is_module_add(path):
    return _module_add_regx.match(path)


_module_prop_add_regx = re.compile(r'.*/modules/\d*/.*$')


def is_module_prop_add(path):
    return _module_prop_add_regx.match(path)


_op_add_regx = re.compile(r'.*/operators/?\d*$')


def is_operator_add(path):
    return _op_add_regx.match(path)


_datasource_add_regx = re.compile(r'.*/dataSources/?\d*$')


def is_datasource_add(path):
    return _datasource_add_regx.match(path)


_op_remove_regx = re.compile(r'.*/operators/?\d*$')


def is_operator_remove(path):
    return _op_remove_regx.match(path)


_module_remove_regx = re.compile(r'.*/modules/?\d*$')


def is_module_remove(path):
    return _module_remove_regx.match(path)


_ds_remove_regx = re.compile(r'.*/dataSources/?\d*$')


def is_datasource_remove(path):
    return _ds_remove_regx.match(path)


def diff(src, dst):
    # The semantic for Tomviz state is a little different than a simple dict.
    # You can't change a operator or module by simply patch id of a dict. If
    # an id is different then the objects should be treated as immutable. So we
    # need to post process the operations and replace any attempts to patch
    # object ids with a operation to replace the whole object.

    # Utility function to get the prefix from a path
    def _prefix(p):
        return '/'.join(p.split('/')[:-1])

    # The new patch
    patch = []

    # The set of paths the we can skip over, as we are doing a full object
    # replace
    exclude_paths = set()

    # First get the patch from jsonpatch
    original_patch = jsonpatch.JsonPatch.from_diff(src, dst)

    # Iterate once to build up list of paths we can discard as we will be
    # simple replacing the whole object. We need todo this in two iterations as
    # we can't guarantee that the replace operation for a id will appear before
    # other operations that we need to exclude.
    for op in original_patch:
        path_prefix = _prefix(op['path'])

        if op['op'] == 'replace':
            if op['path'].endswith('/id'):
                # Save the path so we know we can ignore operations that start
                # with this path.
                exclude_paths.add(path_prefix)

    # Now iterate through and update and exclude appropriate operations.
    for op in original_patch:
        path_prefix = _prefix(op['path'])
        if op['op'] == 'replace':
            # We are trying to patch an id, replace with operation that
            # replaces the whole object.
            if op['path'].endswith('/id'):

                new_object = resolve_pointer(dst, path_prefix)
                op = {
                    "op": "replace",
                    "path": path_prefix,
                    "value": new_object
                }

                patch.append(op)
                continue

        # If not in exclude path add operation
        if path_prefix not in exclude_paths:
            patch.append(op)

    return patch


def mark_modified(ops, modules):
    ops_to_mark = {}

    for path in ops:
        pindex = pipeline_index(path)
        op_index = path[-1]
        if pindex not in ops_to_mark or op_index < ops_to_mark[pindex][-1]:
            ops_to_mark[pindex] = path

    PipelineStateManager().modified(list(ops_to_mark.values()), list(modules))


def update_app(patch, ops_modified, modules_modified):
    path = patch['path']

    if is_operator_update(path):
        ops_modified.add(operator_update(patch))
    elif is_module_update(path):
        modules_modified.add(module_update(patch))
    elif is_datasource_update(path):
        pass
        # TODO: Deal with changing file Names
        # datasources_modified.add(datasource_update(o))


def add_to_app(patch):
    path = patch['path']

    if is_module_add(path):
        module_add(patch)
    elif is_module_update(path):
        module_update(patch)
    elif is_operator_add(path):
        operator_add(patch)
    elif is_operator_update(path):
        operator_update(patch)
    elif is_datasource_add(path):
        datasource_add(patch)
    elif is_datasource_update(path):
        pass


def remove_from_app(patch):
    path = patch['path']
    if is_operator_remove(path):
        operator_remove(patch)
    elif is_operator_update(path):
        operator_update(patch)
    elif is_module_remove(path):
        module_remove(patch)
    elif is_module_update(path):
        module_update(patch)
    elif is_datasource_remove(path):
        datasource_remove(patch)
    elif is_datasource_update(path):
        pass


def convert_move_app(patch, ops_modified, modules_modified):
    # Just convert to a remove and add
    state = PipelineStateManager().serialize()
    state = json.loads(state)
    value = resolve_pointer(state, patch['from'])
    remove_from_app({
        'op': 'remove',
        'path': patch['from'],
    })

    update_app({
        'op': 'add',
        'path': patch['path'],
        'value': value
    }, ops_modified, modules_modified)


def sync_state_to_app(src, dst):
    patch = diff(src, dst)

    ops_modified = set()
    modules_modified = set()

    PipelineStateManager().disable_sync_to_python()

    for o in patch:
        patch_op = o['op']
        if patch_op == 'replace':
            update_app(o, ops_modified, modules_modified)
        elif patch_op == 'add':
            add_to_app(o)
        elif patch_op == 'remove':
            remove_from_app(o)
        elif patch_op == 'move':
            convert_move_app(o, ops_modified, modules_modified)
        else:
            raise Exception('Unexcepted op type: %s' % o['op'])

    PipelineStateManager().enable_sync_to_python()

    # Sync from app to ensure python as the updated state
    sync_state_to_python()

    mark_modified(ops_modified, modules_modified)


#
# Function for syncing state to Python
#


def operator_update_python(patch_op, removed_cache):
    # First get path to operator
    path = patch_op['path']
    op_path = operator_path(path)

    # Now get the current state of the operator
    op = find_operator(op_path.split('/')[1:])
    current_op_state = dump_operator(op)

    # Apply the update
    # Adjust the path
    patch_op['path'] = path.replace(op_path, '')
    patch = jsonpatch.JsonPatch([patch_op])
    current_op_state = patch.apply(current_op_state)
    new_op = load_operator(current_op_state, removed_cache)

    # Finally update the object inplace
    update(new_op, op)


def add_mod_to_removed_cache(removed_cache, mod):
    removed_cache['modules'][mod.id] = mod


def add_ds_to_removed_cache(removed_cache, ds):
    removed_cache['dataSources'][ds.id] = ds

    for op in ds.operators:
        add_op_to_removed_cache(removed_cache, op)

    for mod in ds.modules:
        add_mod_to_removed_cache(removed_cache, mod)


def add_op_to_removed_cache(removed_cache, op):
    removed_cache['operators'][op.id] = op

    if hasattr(op, 'dataSources'):
        for ds in op.dataSources:
            add_ds_to_removed_cache(removed_cache, ds)


def operator_remove_from_python(patch_op, removed_cache):
    # First get path to operator
    path = patch_op['path']
    op_path = operator_path(path)

    # Find parent data source
    ds = find_datasource(op_path.split('/')[1:])

    # Now get the the operator
    op = find_operator(op_path.split('/')[1:])

    # Remove from data source
    ds.operators.remove(op)

    add_op_to_removed_cache(removed_cache, op)


def operator_add_to_python(patch_op, removed_cache):
    # First get path to operator
    path = patch_op['path']
    op_path = operator_path(path)

    # Find parent data source
    ds = find_datasource(op_path.split('/')[1:])

    # Load the new ops
    value = patch_op['value']
    ops = value if isinstance(value, list) else [value]
    for op in ops:
        op = load_operator(op, removed_cache)
        ds.operators.append(op)


def module_update_python(patch_module):
    # First get path to module
    path = patch_module['path']
    mod_path = module_path(path)

    # Now get the current state of the module
    module = find_module(mod_path.split('/')[1:])
    current_module_state = dump_module(module)

    # Apply the update
    # Adjust the path
    patch_module['path'] = path.replace(mod_path, '')
    patch = jsonpatch.JsonPatch([patch_module])

    current_module_state = patch.apply(current_module_state)
    new_module = load_module(current_module_state)

    # Finally update the object inplace
    update(new_module, module)


def module_remove_from_python(patch_op, removed_cache):
    # First get path to module
    path = patch_op['path']
    mod_path = module_path(path)

    # Find parent data source
    ds = find_datasource(mod_path.split('/')[1:])

    # Now get the the module
    mod = find_module(mod_path.split('/')[1:])

    # Remove from data source
    ds.modules.remove(mod)

    # Add to cache, in case we need to resurrect it
    add_mod_to_removed_cache(removed_cache, mod)

    # Kill it
    # mod._kill()


def module_add_to_python(patch_module, removed_cache):
    # First get path to module
    path = patch_module['path']
    mod_path = module_path(path)

    # Find parent data source
    ds = find_datasource(mod_path.split('/')[1:])

    # Load the new modules
    value = patch_module['value']
    mods = value if isinstance(value, list) else [value]
    for mod in mods:
        mod = load_module(mod, removed_cache)
        ds.modules.append(mod)


def datasource_update_python(patch_ds, removed_cache):
    # First get path to module
    path = patch_ds['path']
    ds_path = datasource_path(path)

    # Now get the current state of the datasource
    ds = find_datasource(ds_path.split('/')[1:])
    current_ds_state = dump_datasource(ds)

    # Apply the update
    # Adjust the path
    patch_ds['path'] = path.replace(ds_path, '')
    patch = jsonpatch.JsonPatch([patch_ds])

    current_ds_state = patch.apply(current_ds_state)
    new_ds = load_datasource(current_ds_state, removed_cache)

    # Finally update the object inplace
    update(new_ds, ds)


def datasource_remove_from_python(patch_module, removed_cache):
    from . import pipelines

    # First get path to datasource
    path = patch_module['path']
    ds_path = datasource_path(path)

    # Find parent data source
    ds = find_datasource(ds_path.split('/')[1:])

    # Remove from pipelines
    pipeline = None
    for p in pipelines:
        if p.dataSource == ds:
            pipeline = p
            break

    if pipeline is None:
        raise Exception('Unable to find data source.')

    pipelines.remove(pipeline)

    # Kill them!
    pipeline._kill()

    add_ds_to_removed_cache(removed_cache, ds)


def datasource_add_to_python(patch_datasource, removed_cache):
    from . import pipelines
    # Load the new datasources
    value = patch_datasource['value']
    data_sources = value if isinstance(value, list) else [value]

    for ds in data_sources:
        ds = load_datasource(ds, removed_cache)
        pipelines.append(Pipeline(ds))


def add_to_python(patch, remove_cache):
    path = patch['path']

    if is_module_add(path):
        module_add_to_python(patch, remove_cache)
    elif is_module_update(path):
        module_update_python(patch)
    elif is_operator_add(path):
        operator_add_to_python(patch, remove_cache)
    elif is_operator_update(path):
        operator_update_python(patch, remove_cache)
    elif is_datasource_add(path):
        datasource_add_to_python(patch, remove_cache)
    elif is_datasource_update(path):
        pass


def update_python(patch, remove_cache):
    path = patch['path']
    if is_operator_update(path):
        operator_update_python(patch, remove_cache)
    if is_module_update(path):
        module_update_python(patch)
    elif is_datasource_update(path):
        pass


def remove_from_python(patch, removed_cache):
    path = patch['path']
    if is_module_remove(path):
        module_remove_from_python(patch, removed_cache)
    elif is_module_update(path):
        module_update_python(patch)
    elif is_operator_remove(path):
        operator_remove_from_python(patch, removed_cache)
    elif is_operator_update(path):
        operator_update_python(patch, removed_cache)
    elif is_datasource_remove(path):
        datasource_remove_from_python(patch, removed_cache)
    elif is_datasource_update(path):
        datasource_update_python(patch, removed_cache)


def convert_move_python(patch, removed_cache):
    from . import _current_state
    state = _current_state()
    # Just convert to a remove and add
    value = resolve_pointer(state, patch['from'])
    remove_from_python({
        'op': 'remove',
        'path': patch['from'],
    }, removed_cache)

    update_python({
        'op': 'add',
        'path': patch['path'],
        'value': value
    }, removed_cache)


def sync_state_to_python(current_python_state=None, current_app_state=None):
    from . import _current_state

    if current_python_state is None:
        current_python_state = _current_state()

    if current_app_state is None:
        current_app_state = PipelineStateManager().serialize()
        current_app_state = json.loads(current_app_state)

    patch = diff(current_python_state, current_app_state)

    removed_cache = {
        'modules': {},
        'operators': {},
        'dataSources': {}
    }

    for o in patch:
        patch_op = o['op']
        if patch_op == 'replace':
            update_python(o, removed_cache)
        elif patch_op == 'add':
            add_to_python(o, removed_cache)
        elif patch_op == 'remove':
            remove_from_python(o, removed_cache)
        elif patch_op == 'move':
            convert_move_python(o, removed_cache)
        else:
            raise Exception('Unexpected op type: %s' % o['op'])

    # Now kill off any removed objects
    for m in removed_cache['modules'].values():
        m._kill()

    for o in removed_cache['operators'].values():
        o._kill()

    for d in removed_cache['dataSources'].values():
        d._kill()

import jsonpatch
import re
import json

from ._jsonpath import (
    operator_path,
    module_path,
    datasource_path,
    pipeline_index,
    find_operator,
    find_module,
    find_datasource
)

from ._pipeline import PipelineStateManager


def operator_replace(patch_op):
    # First get path to operator
    path = patch_op['path']
    op_path = operator_path(path)

    # Now get the current state of the operator
    op = find_operator(op_path.split('/')[1:]);
    current_state = PipelineStateManager().serialize_op(path, op.id)
    current_state_dct = json.loads(current_state)

    # Apply the update
    # Adjust the path
    patch_op['path'] = path.replace(op_path, '')
    patch = jsonpatch.JsonPatch([patch_op])
    current_state_dct = patch.apply(current_state_dct)

    # Now update the operator
    PipelineStateManager().deserialize_op(op_path, json.dumps(current_state_dct))

    return op_path

def module_replace(patch_module):
    # First get path to module
    path = patch_module['path']
    module_path = module_path(path)

    # Now get the current state of the operator
    module = find_module(module_path.split('/')[1:])
    current_state = PipelineStateManager().serialize_module(path, module.id)
    current_state_dct = json.loads(current_state)

    # Apply the update
    # Adjust the path
    patch_module['path'] = path.replace(module_path, '')
    patch = jsonpatch.JsonPatch([patch_module])
    current_state_dct = patch.apply(current_state_dct)

    # Now update the module
    PipelineStateManager().deserialize_module(module_path, json.dumps(current_state_dct))

    return module_path

def module_add(patch_module):
    if patch_module['value'] == []:
        return

    module_json = patch_module['value']
    if isinstance(module_json, list):
        module_json = module_json[0]
    path = patch_module['path']
    ds_path = datasource_path(path)
    data_source = find_datasource(ds_path.split('/')[1:])

    state = PipelineStateManager().add_module(ds_path, data_source.id, module_json['type'])
    # If we are adding our first module just find the datasource and the first module
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
    PipelineStateManager().deserialize_module(path, state['id'], json.dumps(state))

def operator_add(patch_op):
    if patch_op['value'] == []:
        return

    op_json = patch_op['value']
    if isinstance(op_json, list):
        op_json = op_json[0]
    path = patch_op['path']
    ds_path = datasource_path(path)
    data_source = find_datasource(ds_path.split('/')[1:])

    PipelineStateManager().add_operator(ds_path, data_source.id, json.dumps(op_json))

_datasource_regx = re.compile(r'.*/dataSources/\d(.*)')
def is_datasource_update(path):
    datasource_match = _datasource_regx.match(path)

    return  (datasource_match and
        (is_module_update(datasource_match.group(1)) is None) and
        (is_operator_update(datasource_match.group(1)) is None))

_module_regx = re.compile(r'.*/modules/\d(.*)')
def is_module_update(path):
    module_match = _module_regx.match(path)

    return module_match and (is_datasource_update(module_match.group(1)) is None)

_op_regx = re.compile(r'.*/operators/\d(.*)')
def is_operator_update(path):
    op_match = _op_regx.match(path)

    return op_match and (is_datasource_update(op_match.group(1)) is None)

_module_add_regx = re.compile(r'.*/modules/?\d*$')
def is_module_add(path):
    return _module_add_regx.match(path)

_op_add_regx = re.compile(r'.*/operators/?\d*$')
def is_op_add(path):
    return _op_add_regx.match(path)

def diff(src, dst):
    patch = jsonpatch.JsonPatch.from_diff(src, dst)
    patch = json.loads(patch.to_string())

    return patch
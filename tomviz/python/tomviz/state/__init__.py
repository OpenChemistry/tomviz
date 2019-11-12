import json
import jsonpatch
from jsonpointer import resolve_pointer
import sys
import re
from pathlib import Path

from ._schemata import (
    TomvizSchema,
    PipelineSchema,
    DataSourceSchema,
    OperatorSchema,
    ModuleSchema,
    ColorMap2DBoxSchema,
    ColorOpacityMap,
    GradientOpacityMap
)

from ._models import (
    Module,
    Operator,
    ModuleMeta,
    OperatorMeta,
    init_operators,
    init_modules,
)

from ._jsonpath import (
    operator_path,
    module_path,
    datasource_path,
    pipeline_index
)

from ._jsonpatch import (
    diff,
    is_datasource_update,
    is_module_update,
    is_operator_update,
    is_module_add,
    is_op_add,
    operator_replace,
    module_replace,
    operator_add,
    module_add
)

from ._pipeline import PipelineStateManager

pipelines = []
t = None

def _mark_modified(ops, modules):
    ops_to_mark = {}

    for path in ops:
        pindex = pipeline_index(path)
        op_index = path[-1]
        if pindex not in ops_to_mark or op_index < ops_to_mark[pindex][-1]:
            ops_to_mark[pindex] = path

    PipelineStateManager().modified(list(ops_to_mark.values()), list(modules))

def _update_state(src, dst):
    patch = diff(src, dst)

    ops_modified = set()
    modules_modified = set()

    for o in patch:
        path = o['path']
        if o['op'] == 'replace':
            if is_operator_update(path):
                ops_modified.add(operator_replace(o))
            elif is_module_update(path):
                modules_modified.add(module_replace(o))
        elif o['op'] == 'add':
            if is_module_add(path):
                module_add(o)
            elif is_op_add(path):
                operator_add(o)

    _mark_modified(ops_modified, modules_modified)

_state = None

def _init():
    global pipelines
    global _state
    _state = PipelineStateManager().serialize()
    _state = json.loads(_state)
    schema = TomvizSchema()

    global t
    t = schema.load(_state)
    pipelines.clear()
    pipelines += t.pipelines

def sync():
    global _state

    schema  = TomvizSchema()
    new_state = schema.dump(t)

    _update_state(_state, new_state)
    _state = new_state

def reset():
    _init()

def load(state, state_dir=None):
    def _load_from_path(path):
        nonlocal state_dir
        state_dir = str(path.parent)
        with path.open('r') as fp:
            state = fp.read();

        return state

    if isinstance(state, str):
        path = Path(state)
        state = _load_from_path(path)
    elif isinstance(state, Path):
        state = _load_from_path(state)
    elif isinstance(state, dict):
        state = json.dumps(state)
    elif hasattr(state, 'read'):
        state = state.read()

    if state_dir is None:
        raise Exception("'state_dir' must be provided inorder to locate data associated with state file.")

    PipelineStateManager().load(state, state_dir)


init_modules()
init_operators()


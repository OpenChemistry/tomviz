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
    Tomviz,
    Pipeline,
    Module,
    Operator,
    DataSource,
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
    sync_state_to_app,
    sync_state_to_python
)

from ._pipeline import PipelineStateManager

t = None
pipelines = None



_state = None

def _init():
    global pipelines
    global _state
    _state = PipelineStateManager().serialize()
    _state = json.loads(_state)
    schema = TomvizSchema()

    global t
    t = schema.load(_state)
    pipelines = t.pipelines

def _sync_to_python(pipeline_state):
    global _state
    schema  = TomvizSchema()
    _state = schema.dump(t)

    sync_state_to_python(_state, json.loads(pipeline_state))

def sync():
    global _state

    schema  = TomvizSchema()
    new_state = schema.dump(t)

    sync_state_to_app(_state, new_state)
    _state = schema.dump(t)#new_state

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

def _current_state():
    global t
    schema  = TomvizSchema()
    return schema.dump(t)

init_modules()
init_operators()
_init()


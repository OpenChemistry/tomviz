import json
from pathlib import Path

from ._schemata import (
    TomvizSchema,
)

from ._models import ( # noqa
    Pipeline,
    DataSource,
    init_operators,
    init_modules,
)

from ._jsonpatch import (
    sync_state_to_app,
    sync_state_to_python
)

from ._pipeline import PipelineStateManager

from ._views import (
    View
)

from paraview.simple import (
    GetViews,
    GetActiveView,
)

t = None
pipelines = None
views = []
active_view = None
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
    schema = TomvizSchema()
    _state = schema.dump(t)

    sync_state_to_python(_state, json.loads(pipeline_state))

    _state = schema.dump(t)


def _current_state():
    global t
    schema = TomvizSchema()
    return schema.dump(t)


def _pipeline_index(ds):
    for (i, p) in enumerate(pipelines):
        if p.dataSource == ds:
            return i
            break

    return -1


def _active_view():
    return View(GetActiveView())


def _views():
    return [View(v) for v in GetViews()]


views = _views()
active_view = _active_view()


def _sync_views():
    global views
    global active_view

    views = _views()
    active_view = _active_view()


def sync():
    global _state

    schema = TomvizSchema()
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
            state = fp.read()

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
        raise Exception("'state_dir' must be provided inorder to locate data "
                        "associated with state file.")

    PipelineStateManager().load(state, state_dir)


init_modules()
init_operators()
_init()

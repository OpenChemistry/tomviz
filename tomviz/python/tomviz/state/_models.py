import json

from . import operators, modules

from ._pipeline import PipelineStateManager

op_class_attrs = ['description', 'label', 'script', 'type']

class OperatorMeta(type):
    def __init__(cls, name, bases, dict):
        attrs = {}

        for k, v in dict.items():
            if k not in op_class_attrs:
                attrs[k] = property(lambda self: getattr(self, k), lambda self, value: setattr(self, k, value))
            else:
                attrs[k] = v

        super(OperatorMeta, cls).__init__(name, bases, attrs)


class ModuleMeta(type):
    def __new__(meta, name, parents, dct):
        attrs = {}

        attrs = {
            '_props': dct
        }

        # for k, v in dct.items():
        #     def _set(self, value, key=k):
        #         setattr(self, '_%s' % key, value)

        #     def _get(self, key=k):
        #         return getattr(self, '_%s' % key)
        #     attrs[k] = property(_get, _set)
        #     attrs['_%s' % k] = v

        return super(ModuleMeta, meta).__new__(meta, name, parents, attrs)


class Pipeline(object):
    def __init__(self, datasource):
        self._datasource = datasource

    @property
    def datasource(self):
        return self._datasource


class DataSource(object):
    def __init__(self, **kwargs):
        #self.operators = []
        #self.modules = []
        for k, v in kwargs.items():
            setattr(self, k, v)

    def add_module(self, module):
        self.modules.append(module)

class Operator(object):
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            setattr(self, k, v)

class Module(object):
    def __init__(self, **kwargs):
        args = self._props
        args.update(kwargs)

        for k,v in args.items():
            if isinstance(v, dict):
                v = to_namespaces(v)
            setattr(self, k, v)

    def _updates(self):
        current_state = serialize_module(self)
        patch = jsonpatch.JsonPatch.from_diff(self._props, current_state)
        patch = json.loads(patch.to_string())

        return patch

class Tomviz(object):
    def __init__(self, pipelines):
        self._pipelines = pipelines

    @property
    def pipelines(self):
        return self._pipelines

def module_json_to_classes(module_json):
    for name, info in module_json.items():
        info['type'] = name
        del info['id']
        if not hasattr(modules, name):
            cls = ModuleMeta(name, (Module,), info)
            setattr(modules, name, cls)

def operator_json_to_classes(operator_json):
    for name, info in operator_json.items():
        del info['id']
        if not hasattr(operators, name):
            cls = OperatorMeta(name, (Operator,), info)
            setattr(operators, name, cls)

def init_modules():
    module_json_str = PipelineStateManager().module_json()
    module_json = json.loads(module_json_str)
    module_json_to_classes(module_json)

def init_operators():
    operator_json_str = PipelineStateManager().operator_json()
    operator_json = json.loads(operator_json_str)
    operator_json_to_classes(operator_json)
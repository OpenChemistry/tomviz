import collections
import json
from types import SimpleNamespace

from marshmallow import fields, Schema, post_load, EXCLUDE, INCLUDE, pre_load, post_dump

from ._models import (
    Tomviz,
    Pipeline,
    DataSource,
    Operator,
    Module
)

from . import operators, modules
from ._models import op_class_attrs

def iter_paths(tree, parent_path=()):
    for path, node in tree.items():
        current_path = parent_path + (path,)
        if isinstance(node, collections.Mapping):
            for inner_path in iter_paths(node, current_path):
                yield inner_path
        else:
            yield current_path

def to_namespaces(dct):
    root = SimpleNamespace()
    for path in iter_paths(dct):
        prop_name = path[-1]
        path = path[:-1]
        current_namespace = root

        for p in path:
            if hasattr(current_namespace, p):
                current_namespace = getattr(current_namespace, p)
            else:
                new_namespace = SimpleNamespace()
                setattr(current_namespace, p, new_namespace)
                current_namespace = new_namespace

        v = dct
        for k in path + (prop_name,):
            v = v[k]

        setattr(current_namespace, prop_name, v)

    return root

def from_namespace(namespace):
    d = {}

    for a in attrs(namespace):
        v = getattr(namespace, a)
        if attrs(v):
            d[a] = from_namespace(v)
        else:
            d[a] = v

    return d

def attrs(o):
    return [x for x in dir(o) if not x.startswith('_') and not callable(getattr(o, x)) and not isinstance(o, (int, float, str, dict)) ]

class OperatorSchema(fields.Field):
    def _serialize(self, value, attr, obj, **kwargs):
        if value is None:
            return ""

        state = {}
        for a in dir(value):
            if a.startswith('_') or a == 'name':
                continue

            v = getattr(value, a)
            #if a in op_class_attrs:
                #if a == 'description':
                #    v = json.dumps(v)
                #state[a] = v
            if a == 'dataSources':
                s = DataSourceSchema()
                v = [s.dump(d) for d in v]
                state[a] = v
            elif attrs(v):
                state[a] = from_namespace(v)
            else:
                state[a] = v

        return state

    def _deserialize(self, value, attr, data, **kwargs):
        try:
            description = json.loads(value['description'])
            name = description['name']
        except (json.decoder.JSONDecodeError, KeyError):
            name = ''.join([x.capitalize() for x in value['label'].split(' ')])
        cls = getattr(operators, name)

        args = {}
        if 'arguments' in value:
            args['arguments'] = to_namespaces(value['arguments'])
        if 'dataSources' in value:
            s = DataSourceSchema()
            datasources = value['dataSources']
            datasources = [s.load(d) for d in datasources]
            args['dataSources']= datasources
        args['id'] = value['id']

        return  cls(**args)

class TestSchema(Schema):
    test = fields.List(OperatorSchema)

class ModuleField(fields.Field):
    def _serialize(self, value, attr, obj, **kwargs):
        if value is None:
            return ""

        state = {
            'type': value.__class__.__name__
        }
        for a in dir(value):
            if a.startswith('_'):
                continue

            v = getattr(value, a)
            if attrs(v):
                v = from_namespace(v)

            state[a] = v

        return state

    def _deserialize(self, value, attr, data, **kwargs):
        cls = getattr(modules, value['type'])

        return  cls(**value)

class ModuleSchema(Schema):
    module = fields.List(ModuleField)


class ColorMap2DBoxSchema(Schema):
    x = fields.Integer()
    y = fields.Integer()
    height = fields.Float()
    width = fields.Float()
    @post_load
    def make_datasource(self, data, **kwargs):
        return to_namespaces(data)

    class Meta:
        unknown = INCLUDE

class ColorOpacityMap(Schema):
    colorSpace = fields.String()
    colors = fields.List(fields.Float)
    points = fields.List(fields.Float)

    @post_load
    def make_datasource(self, data, **kwargs):
        return to_namespaces(data)

    class Meta:
        unknown = INCLUDE

class GradientOpacityMap(Schema):
    points = fields.List(fields.Float)

    @post_load
    def make_datasource(self, data, **kwargs):
        return to_namespaces(data)

    class Meta:
        unknown = INCLUDE


class DataSourceSchema(Schema):
    operators = fields.List(OperatorSchema, missing=[])
    colorMap2DBox = fields.Nested(ColorMap2DBoxSchema)
    colorOpacityMap = fields.Nested(ColorOpacityMap)
    gradientOpacityMap = fields.Nested(GradientOpacityMap)
    modules = fields.List(ModuleField, missing=[])
    useDetachedColorMap = fields.Boolean()
    id = fields.String()
    label = fields.String()

    @post_load
    def make_datasource(self, data, **kwargs):
        return DataSource(**data)

    @post_dump
    def remove_empty(self, data, **kwargs):
        remove_if_empty = ['operators', 'modules']

        return {
            k:v for k,v in data.items() if k not in remove_if_empty or v != []
        }

    class Meta:
        unknown = EXCLUDE


class PipelineSchema(Schema):
    datasource = fields.Nested(DataSourceSchema)

    # The following @pre_load and @post_dump are need to allow use to have
    # a Pipeline object with a datasource attribute. The JSON object in
    # the serialized state that represents a pipeline has not such attribute.
    # Using these two hooks allows use to acheive the object graph we want from
    # the serialize state.
    @pre_load
    def wrap_datasource(self, data, **kwargs):
        """
        Add the DataSource as an attribute of the pipeline.
        """
        return {
            'datasource': data
        }

    @post_dump
    def unwrap_datasource(self, data, **kwargs):
        """
        Extract DataSource from pipeline attribute.
        """

        return data['datasource']

    @post_load
    def make_pipeline(self, data, **kwargs):
        return Pipeline(data['datasource'])

    class Meta:
        unknown = EXCLUDE


class TomvizSchema(Schema):
    pipelines = fields.List(fields.Nested(PipelineSchema), data_key='dataSources')

    @post_load
    def make_tomviz(self, data, **kwargs):
        return Tomviz(data['pipelines'])

    class Meta:
        unknown = EXCLUDE

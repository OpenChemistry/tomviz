import json
import types

from marshmallow import fields, Schema, post_load, EXCLUDE, INCLUDE, pre_load, post_dump

from ._models import (
    Tomviz,
    Pipeline,
    DataSource,
    Operator,
    Module,
    Reader
)

from ._utils import (
    to_namespaces,
    from_namespace,
    attrs
)

from tomviz import operators, modules
from ._models import op_class_attrs


class OperatorField(fields.Field):
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
        from ._jsonpatch import update
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
            s = DataSourceSchema(context=self.context)
            datasources = value['dataSources']
            datasources = [s.load(d) for d in datasources]
            args['dataSources']= datasources
        args['id'] = value['id']

        removed_cache = self.context.get('operators', {})
        op = cls(**args)

        # Do we a op that we can recurrect?
        if op.id in removed_cache:
            op = update(op, removed_cache.pop(op.id))

        return  op

class OperatorSchema(Schema):
    operator = OperatorField()

def load_operator(operator, removed_cache=None):
    schema = OperatorSchema()
    if removed_cache is not None:
        schema.context = removed_cache

    return schema.load({
        'operator': operator
    })['operator']

def dump_operator(op):
    ns = types.SimpleNamespace(operator=op)
    return OperatorSchema().dump(ns)['operator']

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
        from ._jsonpatch import update
        cls = getattr(modules, value['type'])

        removed_cache = self.context.get('modules', {})
        mod = cls(**value)

        # Do we a module that we can recurrect?
        if mod.id in removed_cache:
            mod = update(mod, removed_cache.pop(mod.id))

        return  mod

class ModuleSchema(Schema):
    module = ModuleField()

def load_module(module, removed_cache=None):
    schema = ModuleSchema()
    if removed_cache is not None:
        schema.context = removed_cache
    return schema.load({
        'module': module
    })['module']

def dump_module(module):
    ns = types.SimpleNamespace(module=module)
    return ModuleSchema().dump(ns)['module']

class ColorMap2DBoxSchema(Schema):
    x = fields.Float()
    y = fields.Float()
    height = fields.Float()
    width = fields.Float()
    @post_load
    def make_namespace(self, data, **kwargs):
        return to_namespaces(data)

    class Meta:
        unknown = INCLUDE

class ColorOpacityMap(Schema):
    colorSpace = fields.String()
    colors = fields.List(fields.Float)
    points = fields.List(fields.Float)

    @post_load
    def make_namespace(self, data, **kwargs):
        return to_namespaces(data)

    class Meta:
        unknown = INCLUDE

class GradientOpacityMap(Schema):
    points = fields.List(fields.Float)

    @post_load
    def make_namespace(self, data, **kwargs):
        return to_namespaces(data)

    class Meta:
        unknown = INCLUDE

class ReaderSchema(Schema):
    fileNames = fields.List(fields.String)
    name = fields.String()

    @post_load
    def make_reader(self, data, **kwargs):
        return Reader(**data)

    @post_dump
    def remove_empty(self, data, **kwargs):
        if data['name'] is None:
            del data['name']

        return data

class DataSourceSchema(Schema):
    operators = fields.List(OperatorField, missing=[])
    colorMap2DBox = fields.Nested(ColorMap2DBoxSchema)
    colorOpacityMap = fields.Nested(ColorOpacityMap)
    gradientOpacityMap = fields.Nested(GradientOpacityMap)
    modules = fields.List(ModuleField, missing=[])
    useDetachedColorMap = fields.Boolean()
    spacing = fields.List(fields.Float)
    units = fields.String()
    id = fields.String()
    label = fields.String()
    reader = fields.Nested(ReaderSchema)
    active = fields.Boolean()

    @post_load
    def make_datasource(self, data, **kwargs):
        from ._jsonpatch import update

        ds = DataSource(**data)
        # Has this data source been removed, if so recurrent it,
        # rather than creating a new one.
        removed_cache = self.context.get('dataSources', {})
        if ds.id in removed_cache:
            update(ds, removed_cache.pop(ds.id))

        return ds

    @post_dump
    def remove_empty(self, data, **kwargs):
        remove_if_empty = ['operators', 'modules']

        return {
            k:v for k,v in data.items() if k not in remove_if_empty or v != []
        }

    class Meta:
        unknown = EXCLUDE

def load_datasource(datasource, removed_cache=None):
    schema = DataSourceSchema()
    if removed_cache is not None:
        schema.context = removed_cache

    return DataSourceSchema().load(datasource)

def dump_datasource(datasource):
    return DataSourceSchema().dump(datasource)

class PipelineSchema(Schema):
    dataSource = fields.Nested(DataSourceSchema)

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
            'dataSource': data
        }

    @post_dump
    def unwrap_datasource(self, data, **kwargs):
        """
        Extract DataSource from pipeline attribute.
        """

        return data['dataSource']

    @post_load
    def make_pipeline(self, data, **kwargs):
        return Pipeline(data['dataSource'])

    class Meta:
        unknown = EXCLUDE


class TomvizSchema(Schema):
    pipelines = fields.List(fields.Nested(PipelineSchema), data_key='dataSources')

    @post_load
    def make_tomviz(self, data, **kwargs):
        return Tomviz(data['pipelines'])

    class Meta:
        unknown = EXCLUDE

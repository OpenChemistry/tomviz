import os
import sys
import tempfile
import importlib
import inspect
import logging
import bottle
from bottle import run, route

from tomviz import jsonrpc
from tomviz.utility import inject
from tomviz.acquisition import AbstractSource
import shutil

dev = False
adapter = 'tests.mock.source.ApiAdapter'
host = 'localhost'
port = 8080
logger = logging.getLogger('tomviz')


def _load_source_adapter(source_adapter):
    if source_adapter is None:
        source_adapter = adapter
    logger.info('Loading source_adapter: %s', source_adapter)
    # First load the chosen source_adapter
    module, cls = source_adapter.rsplit('.', 1)
    try:
        imported = importlib.import_module(module)
    except ImportError:
        logger.error('Unable to load module: %s', module)
        raise

    try:
        cls = getattr(imported, cls)
    except AttributeError:
        logger.error('Unable to get constructor for: %s', source_adapter)
        raise

    return cls


def setup_app(source_adapter=None):
    cls = _load_source_adapter(source_adapter)

    # Ensure that the adapter implements the interface
    bases = inspect.getmro(cls)
    if len(bases) < 2 or bases[1] != AbstractSource:
        logger.error('Adapter %s, does not derive from %s', source_adapter,
                     '.'.join([AbstractSource.__module__,
                               AbstractSource.__name__]))

    source_adapter = cls()
    slices = {}

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def describe(source_adapter, method):

        if not hasattr(source_adapter, method):
            raise Exception('Method %s not found.' % method)

        func = getattr(source_adapter, method)

        description = []
        if hasattr(func, 'description'):
            description = getattr(func, 'description')

        return description

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def connect(source_adapter, **params):
        return source_adapter.connect(**params)

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def disconnect(source_adapter, **params):
        return source_adapter.disconnect(**params)

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def tilt_params(source_adapter, **params):
        return source_adapter.tilt_params(**params)

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def acquisition_params(source_adapter, **params):
        return source_adapter.acquisition_params(**params)

    def _base_url():
        return '%s://%s' % (bottle.request.urlparts.scheme,
                            bottle.request.urlparts.netloc)

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def preview_scan(source_adapter):
        id = 'preview_scan_slice'
        slices[id] = source_adapter.preview_scan()
        return '%s/data/%s' % (_base_url(), id)

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def stem_acquire(source_adapter):
        id = 'stem_acquire_slice'
        slices[id] = source_adapter.stem_acquire()

        return '%s/data/%s' % (_base_url(), id)

    @route('/data/<id>')
    def data(id):
        bottle.response.headers['Content-Type'] = 'image/tiff'

        if id not in slices:
            bottle.response.status = 404
            return 'Acquisition data not found.'

        return slices[id]

def deploy_module(name, source):
    try:
        tmp_dir = tempfile.mkdtemp()

        sys.path.append(tmp_dir)


        module_path = '%s.py' % os.path.join(tmp_dir, name)
        with open(module_path, 'w') as fp:
            fp.write(source)

        if name in sys.modules:
            reload(sys.modules[name])
        else:
            importlib.import_module(name)

    finally:
        sys.path.remove(tmp_dir)
        shutil.rmtree(tmp_dir)

def deploy_adapter(module, name, source):
    deploy_module(module, source)
    adapter_name = '%s.%s' % (module, name)
    setup_app(adapter_name)

def setup(adapter=None):
    if dev:
        jsonrpc.endpoint(path='/dev')(deploy_adapter)
    else:
        setup_app(adapter)

def start(debug=True):
    setup(adapter)
    logger.info('Starting HTTP server')
    run(host='localhost', port=port, debug=debug)

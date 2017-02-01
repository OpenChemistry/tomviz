import importlib
import bottle
from bottle import run, route

from tomviz import jsonrpc
from tomviz.utility import inject

adapter = 'tests.mock.source.ApiAdapter'
host = 'localhost'
port = 8080


def setup_app():

    # First load the chosen adapter
    module, cls = adapter.rsplit('.', 1)
    try:
        imported = importlib.import_module(module)
    except ImportError:
        # TODO add logging
        raise

    try:
        constructor = getattr(imported, cls)
    except AttributeError:
        # TODO add logging
        raise

    source_adapter = constructor()
    slices = {}

    @jsonrpc.endpoint(path='/acquisition')
    @inject(source_adapter)
    def set_tilt_angle(source_adapter, angle):
        return source_adapter.set_tilt_angle(angle)

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


def start():
    setup_app()
    run(host='localhost', port=port, debug=True)

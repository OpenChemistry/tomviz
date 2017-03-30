import pytest
import requests
import time
from threading import Thread
from bottle import default_app, WSGIRefServer

from tomviz.acquisition import server


class Server(Thread):
    def __init__(self, dev=False, port=9999):
        super(Server, self).__init__()
        self.host = 'localhost'
        self.port = port
        self.base_url = 'http://%s:%d' % (self.host, self.port)
        self.url = '%s/acquisition' % self.base_url
        self.dev = dev
        self._server = WSGIRefServer(host=self.host, port=self.port)

    def run(self):
        self.setup()
        self._server.run(app=default_app())

    def start(self):
        super(Server, self).start()
        # Wait for bottle to start
        while True:
            try:
                requests.get(self.base_url)
                break
            except requests.ConnectionError:
                time.sleep(0.1)

    def setup(self, adapter=None):
        server.setup(dev=self.dev, adapter=adapter)

    def stop(self):
        self._server.srv.shutdown()
        # Force the socket to close so we can reuse the same port
        self._server.srv.socket.close()


@pytest.fixture(scope="module")
def acquisition_server():
    srv = Server()
    srv.start()
    yield srv
    srv.stop()
    srv.join()


@pytest.fixture(scope="module")
def acquisition_dev_server():
    srv = Server(dev=True, port=9998)
    srv.start()
    yield srv
    srv.stop()
    srv.join()

import requests
import hashlib
import os
import tempfile
import pytest
import inspect

from tomviz.jsonrpc import jsonrpc_message
from tests.mock.source import ApiAdapter


def test_invalid_content_type(acquisition_server):
    response = requests.post(acquisition_server.url, data='test')

    expected = jsonrpc_message({
        'id': None,
        'error': {
            'message': 'Invalid content type.',
            'data': 'text/plain',
            'code': -32700
        }
    })
    assert response.json() == expected


def test_invalid_json(acquisition_server):
    headers = {'content-type': 'application/json'}
    response = requests.post(acquisition_server.url, headers=headers,
                             data='test')

    expected = jsonrpc_message({
        'id': None,
        'error': {
            'message': 'Invalid JSON.',
            'code': -32700
        }
    })
    response_json = response.json()
    del response_json['error']['data']
    assert response_json == expected


def test_method_not_found(acquisition_server):
    source = 'test'

    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'test',
        'params': [source]
    })

    response = requests.post(acquisition_server.url, json=request)

    assert response.status_code == 404
    expected = jsonrpc_message({
        'id': id,
        'error': {
            'message':  'Method "test" not found.',
            'data': 'test',
            'code': -32601
        }
    })
    assert response.json() == expected


def test_tilt_params(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'angle': 23
        }
    })

    response = requests.post(acquisition_server.url, json=request)

    assert response.status_code == 200
    expected = jsonrpc_message({
        'id': id,
        'result': 23
    })
    assert response.json() == expected


def test_preview_scan(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'angle': 0
        }
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'preview_scan'
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200

    # Now fetch the image
    url = response.json()['result']
    response = requests.get(url)

    assert response.status_code == 200
    expected = '7d185cd48e077baefaf7bc216488ee49'

    md5 = hashlib.md5()
    md5.update(response.content)
    assert md5.hexdigest() == expected


def test_stem_acquire(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'angle': 0
        }
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'stem_acquire'
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200

    # Now fetch the image
    url = response.json()['result']
    response = requests.get(url)

    assert response.status_code == 200
    expected = '7d185cd48e077baefaf7bc216488ee49'

    md5 = hashlib.md5()
    md5.update(response.content)
    assert md5.hexdigest() == expected

    # Try out of range
    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'angle': 74
        }
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'stem_acquire'
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200

    # Now fetch the image
    url = response.json()['result']
    response = requests.get(url)

    assert response.status_code == 200

    expected = 'ac70e27a7db5710e1433393adeda4940'
    md5 = hashlib.md5()
    md5.update(response.content)
    assert md5.hexdigest() == expected


def test_connection(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect'
    })

    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200
    assert response.json()['result']

    # Now disconnect
    request = jsonrpc_message({
        'id': id,
        'method': 'disconnect'
    })
    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200
    assert not response.json()['result']


def test_acquisition_params(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'acquisition_params',
    })
    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200
    expected = {
        'test': 1,
        'foo': 'foo'
    }
    assert response.status_code == 200
    assert response.json()['result'] == expected

    # Now update
    request = jsonrpc_message({
        'id': id,
        'method': 'acquisition_params',
        'params': {
            'foo': 'bar'
        }
    })
    response = requests.post(acquisition_server.url, json=request)
    expected = {
        'test': 1,
        'foo': 'bar'
    }
    assert response.status_code == 200
    assert response.json()['result'] == expected


def test_describe(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'describe',
        'params': {
            'method': 'acquisition_params'
        }

    })
    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200
    assert response.json()['result'] \
        == ApiAdapter.acquisition_params.description

    # Try describing a method that doesn't exist
    request = jsonrpc_message({
        'id': id,
        'method': 'describe',
        'params': {
            'method': 'no_where_man'
        }

    })
    response = requests.post(acquisition_server.url, json=request)
    expected = {
        'message': 'Method no_where_man not found.',
        'code': -32000
    }
    assert response.status_code == 500
    error = response.json()['error']
    del error['data']
    assert error == expected


def test_describe_adapter(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'describe'
    })
    response = requests.post(acquisition_server.url, json=request)
    assert response.status_code == 200, response.json()
    expected = {
        'name': '%s.%s' % (inspect.getmodule(ApiAdapter).__name__,
                           ApiAdapter.__name__)
    }
    assert response.json()['result'] \
        == expected, response.json()


@pytest.fixture(scope='function')
def sentinel_path1():
    path = os.path.join(tempfile.tempdir, 'adapter_sentinel1')
    yield path
    if os.path.exists(path):
        os.remove(path)


@pytest.fixture(scope='function')
def sentinel_path2():
    path = os.path.join(tempfile.tempdir, 'adapter_sentinel2')
    yield path
    if os.path.exists(path):
        os.remove(path)


def test_deploy(acquisition_dev_server, sentinel_path1, sentinel_path2):
    adapter_path = os.path.join(os.path.dirname(__file__), 'fixtures',
                                'source1.py')
    with open(adapter_path) as fp:
        src = fp.read()

    request = jsonrpc_message({
        'id': 1234,
        'method': 'deploy_adapter',
        'params': ['foo', 'ApiAdapter1', src]
    })

    url = '%s/dev' % acquisition_dev_server.base_url
    response = requests.post(url, json=request)
    assert response.status_code == 200

    magic = '299792458'
    # Call connect and make sure it writes out a file
    request = jsonrpc_message({
        'id': 1234,
        'method': 'connect',
        'params': {
            'magic': magic
        }
    })

    assert not os.path.exists(sentinel_path1)

    response = requests.post(acquisition_dev_server.url, json=request)
    assert response.status_code == 200

    assert os.path.exists(sentinel_path1)
    with open(sentinel_path1) as fp:
        assert fp.read() == magic

    # Now redeploy
    adapter_path = os.path.join(os.path.dirname(__file__), 'fixtures',
                                'source2.py')
    with open(adapter_path) as fp:
        src = fp.read()

    request = jsonrpc_message({
        'id': 1234,
        'method': 'deploy_adapter',
        'params': ['foo', 'ApiAdapter2', src]
    })

    url = '%s/dev' % acquisition_dev_server.base_url
    response = requests.post(url, json=request)
    assert response.status_code == 200

    magic = '299792458'
    # Call connect and make sure it writes out a file
    request = jsonrpc_message({
        'id': 1234,
        'method': 'connect',
        'params': {
            'magic': magic
        }
    })

    assert not os.path.exists(sentinel_path2)

    response = requests.post(acquisition_dev_server.url, json=request)
    assert response.status_code == 200

    assert os.path.exists(sentinel_path2)
    with open(sentinel_path2) as fp:
        assert fp.read() == '%sx2' % magic

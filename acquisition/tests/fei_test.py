import pytest
import requests
import hashlib
import sys
import os

from tomviz_acquisition.jsonrpc import jsonrpc_message

# Add mock modules to path
mock_dir = os.path.join(os.path.dirname(__file__), '..', 'tomviz_acquisition',
                        'acquisition', 'vendors', 'fei', 'mock')
sys.path.append(mock_dir)


@pytest.fixture(scope="module")
def fei_acquisition_server(acquisition_server):
    acquisition_server.setup('tomviz_acquisition.acquisition.vendors.fei.FeiAdapter')
    yield acquisition_server


def test_not_connected(fei_acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'stem_rotation': 2.1
        }
    })

    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 500


def test_tilt_params(fei_acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
    })
    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'angle': 2.1
        }
    })

    response = requests.post(fei_acquisition_server.url, json=request)

    assert response.status_code == 200
    assert response.json()['result'] == 2.1


def test_stem_acquire(fei_acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
    })
    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'tilt_params',
        'params': {
            'angle': 2.1
        }
    })

    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'stem_acquire'
    })

    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 200

    # Now fetch the image
    url = response.json()['result']
    response = requests.get(url)

    assert response.status_code == 200
    expected = '0cdcc5139186b0cbb84042eacfca1a13'

    md5 = hashlib.md5()
    md5.update(response.content)
    assert md5.hexdigest() == expected


def test_acquisition_params(fei_acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
    })
    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 200

    request = jsonrpc_message({
        'id': id,
        'method': 'acquisition_params',
    })
    response = requests.post(fei_acquisition_server.url, json=request)
    assert response.status_code == 200
    expected = {
        'calX': 1.2e-08,
        'calY': 1.2e-08,
        'units': 'nm'
    }
    assert response.json()['result']['size'] == expected

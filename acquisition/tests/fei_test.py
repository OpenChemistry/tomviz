import pytest
import requests
import time
import hashlib

from tomviz.jsonrpc import jsonrpc_message


@pytest.fixture(scope="module")
def fei_acquisition_server(acquisition_server):
    acquisition_server.setup('tomviz.acquisition.vendors.fei.FeiAdapter')
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
            'stem_rotation': 2.1
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
            'stem_rotation': 2.1
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
    expected = '1b9723cd7e9ecd54f28c7dae13e38511'

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
        'dwell_time': 3.1,
        'binning': 10,
        'image_size': 'FULL'
    }
    assert response.json()['result'] == expected

    # Now update
    request = jsonrpc_message({
        'id': id,
        'method': 'acquisition_params',
        'params': {
            'dwell_time': 5.2,
            'binning': 100,
            'image_size': 1
        }
    })
    response = requests.post(fei_acquisition_server.url, json=request)
    expected = {
        'dwell_time': 5.2,
        'binning': 100,
        'image_size': 'ACQIMAGESIZE_HALF'
    }
    assert response.status_code == 200
    assert response.json()['result'] == expected


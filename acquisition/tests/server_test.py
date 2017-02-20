import pytest
import requests
import time
import hashlib

from tomviz.jsonrpc import jsonrpc_message


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
    response = requests.post(acquisition_server.url, headers=headers, data='test')

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


def test_set_title_angle(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'set_tilt_angle',
        'params': [23]
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
        'method': 'set_tilt_angle',
        'params': [0]
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
    expected = '1b9723cd7e9ecd54f28c7dae13e38511'

    md5 = hashlib.md5()
    md5.update(response.content)
    assert md5.hexdigest() == expected


def test_stem_acquire(acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'set_tilt_angle',
        'params': [1]
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
    expected = '2dbadcaa028e763a0a69efd371b48c9d'

    md5 = hashlib.md5()
    md5.update(response.content)
    assert md5.hexdigest() == expected

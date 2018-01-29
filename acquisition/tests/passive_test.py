import pytest
import requests
import hashlib
import sys
import os
import time
from PIL import Image


from tomviz.jsonrpc import jsonrpc_message
from .mock import test_image
from .utility import tobytes

# Add mock modules to path
mock_dir = os.path.join(os.path.dirname(__file__), '..', 'tomviz',
                        'acquisition', 'vendors', 'fei', 'mock')
sys.path.append(mock_dir)


@pytest.fixture(scope="module")
def passive_acquisition_server(acquisition_server):
    acquisition_server.setup('tomviz.acquisition.vendors.passive.PassiveWatchSource')
    yield acquisition_server


def test_connected(passive_acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': '/tmp'
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 200, response.content

    # As we haven't started a mock tilt series write calls to should return noting
    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 200

    result = response.json()['result']
    assert result is None

def test_stem_acquire(passive_acquisition_server, tmpdir, mock_tiltseries_writer):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': str(tmpdir),
            'fileNameRegex': '.*\.tif'
        }
    })
    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 200, response.content

    request = jsonrpc_message({
        'id': id,
        'method': 'stem_acquire'
    })

    tilt_series = []
    for _ in range(0, 1000):
        response = requests.post(passive_acquisition_server.url, json=request)
        assert response.status_code == 200, response.content
        # Not images left to fetch
        result = response.json()['result']
        if result is None:
            continue

        url = result['imageUrl']
        response = requests.get(url)
        tilt_series.append(response.content)

        if len(tilt_series) == mock_tiltseries_writer.series_size:
            break

    # make sure we got all the images
    assert len(tilt_series) ==  mock_tiltseries_writer.series_size

    # Now check we got the write images
    with Image.open(test_image()) as image_stack:
        for i in range(0, image_stack.n_frames):
            md5 = hashlib.md5()
            md5.update(tilt_series[i])

            image_stack.seek(i)
            image_slice = tobytes(image_stack)
            expected_md5 = hashlib.md5()
            expected_md5.update(image_slice)
            assert md5.hexdigest() == expected_md5.hexdigest()

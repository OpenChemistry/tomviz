import pytest
import requests
import hashlib
import sys
import os
import re
from PIL import Image
import dm3_lib as dm3

from tomviz_acquisition.jsonrpc import jsonrpc_message
from .mock import test_image, test_dm3_tilt_series
from tomviz_acquisition.acquisition.utility import tobytes

# Add mock modules to path
mock_dir = os.path.join(os.path.dirname(__file__), '..', 'tomviz',
                        'acquisition', 'vendors', 'fei', 'mock')
sys.path.append(mock_dir)


@pytest.fixture(scope="module")
def passive_acquisition_server(acquisition_server):
    source = 'tomviz_acquisition.acquisition.vendors.passive.PassiveWatchSource'
    acquisition_server.setup(source)
    yield acquisition_server


def test_connected_invalid_path(passive_acquisition_server):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': '/bogus'
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 500, response.content


def test_connected_invalid_regex(passive_acquisition_server, tmpdir):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath,
            'fileNameRegex': 'bogus@@232(]'
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 500, response.content

    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath,
            'fileNameRegex': '.*',
            'fileNameRegexGroups': ['angle'],
            'groupRegexSubstitutions': [{
                'n[': '-'
            }]
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 500, response.content


def test_connected_missing_regex(passive_acquisition_server, tmpdir):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath,
            'fileNameRegexGroups': ['angle'],
            'groupRegexSubstitutions': [{
                'n[': '-'
            }]
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 500, response.content

    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath,
            'fileNameRegex': '.*',
            'groupRegexSubstitutions': [{
                'n[': '-'
            }]
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 500, response.content


def test_connected(passive_acquisition_server, tmpdir):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath
        }
    })

    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 200, response.content

    # As we haven't started a mock tilt series write calls to should
    # return noting
    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 200

    result = response.json()['result']
    assert result is None


def test_tiff_stem_acquire(passive_acquisition_server, tmpdir,
                           mock_tiff_tiltseries_writer):
    id = 1234
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath,
            'fileNameRegex': r'.*\.tif'
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
        # No images left to fetch
        result = response.json()['result']
        if result is None:
            continue

        url = result['imageUrl']
        response = requests.get(url)
        tilt_series.append(response.content)

        if len(tilt_series) == mock_tiff_tiltseries_writer.series_size:
            break

    # make sure we got all the images
    assert len(tilt_series) == mock_tiff_tiltseries_writer.series_size

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


def test_dm3_stem_acquire(passive_acquisition_server, tmpdir,
                          mock_dm3_tiltseries_writer):
    id = 1234
    angle_regex = r'.*_([n,p]{1}[\d,\.]+)degree.*\.dm3'
    request = jsonrpc_message({
        'id': id,
        'method': 'connect',
        'params': {
            'path': tmpdir.strpath,
            'fileNameRegex': angle_regex,
            'fileNameRegexGroups': ['angle'],
            'groupRegexSubstitutions': {
                'angle': [{
                    'n': '-',
                    'p': '+'
                }]
            }
        }
    })
    response = requests.post(passive_acquisition_server.url, json=request)
    assert response.status_code == 200, response.content

    request = jsonrpc_message({
        'id': id,
        'method': 'stem_acquire'
    })

    tilt_series = []
    tilt_series_metadata = []
    for _ in range(0, 1000):
        response = requests.post(passive_acquisition_server.url, json=request)
        assert response.status_code == 200, response.content
        # No images left to fetch
        result = response.json()['result']
        if result is None:
            continue

        url = result['imageUrl']
        response = requests.get(url)
        tilt_series.append(response.content)
        if 'meta' in result:
            tilt_series_metadata.append(result['meta'])

        if len(tilt_series) == mock_dm3_tiltseries_writer.series_size:
            break

    # make sure we got all the images
    assert len(tilt_series) == mock_dm3_tiltseries_writer.series_size

    # Now check we got the write images
    for (i, (filename, fp)) in enumerate(test_dm3_tilt_series()):
        dm3_file = dm3.DM3(fp)
        expected_metadata \
            = {k: v.decode('utf8') for k, v in dm3_file.info.items()}
        expected_metadata['fileName'] = filename
        angle = re.match(angle_regex, filename).group(1)
        angle = angle.replace('n', '-')
        angle = angle.replace('p', '+')
        expected_metadata['angle'] = angle
        assert tilt_series_metadata[i] == expected_metadata

        md5 = hashlib.md5()
        md5.update(tilt_series[i])

        image = Image.fromarray(dm3_file.imagedata)
        tiff_image_data = tobytes(image)
        expected_md5 = hashlib.md5()
        expected_md5.update(tiff_image_data)

        assert md5.hexdigest() == expected_md5.hexdigest()

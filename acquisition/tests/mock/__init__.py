import diskcache
import requests
import tempfile
import os
from bisect import bisect_left


url = 'https://data.kitware.com/api/v1/file'


def angle_to_page(angle):

    if angle < -72 or angle > 73:
        return (None, None)
    angles = range(-73, 74, 2)

    index = bisect_left(angles, angle)
    return (index, angles[index] if index else None)


# Get the test data
def test_image():
    _id = '5893921d8d777f07219fca7e'
    download_url = '%s/%s/download' % (url, _id)
    cache_path = os.path.join(tempfile.gettempdir(), 'tomviz_test_cache')
    cache = diskcache.Cache(cache_path)

    if download_url not in cache:
        response = requests.get(download_url, stream=True)
        response.raise_for_status()
        cache.set(download_url, response.raw, read=True)

    return cache.get(download_url, read=True)


# Get the test black image
def test_black_image():
    _id = '58dd39b28d777f0aef5d8ce5'
    download_url = '%s/%s/download' % (url, _id)
    cache_path = os.path.join(tempfile.gettempdir(), 'tomviz_test_cache')
    cache = diskcache.Cache(cache_path)

    if download_url not in cache:
        response = requests.get(download_url, stream=True)
        response.raise_for_status()
        cache.set(download_url, response.raw, read=True)

    return cache.get(download_url, read=True)


def test_dm3_tilt_series():
    """
    A generator yielding the dm3 files forming a tilt series.
    The files are downloaded from data.kitware.com.
    """
    _ids = [
        '5a69e8408d777f0649e0350f',
        '5a69e8408d777f0649e03515',
        '5a69e8408d777f0649e0351b',
        '5a69e8418d777f0649e03521',
        '5a69e8418d777f0649e03527'
    ]

    cache_path = os.path.join(tempfile.gettempdir(), 'tomviz_test_cache')
    cache = diskcache.Cache(cache_path)

    def gen():
        for _id in _ids:
            file_url = '%s/%s' % (url, _id)
            response = requests.get(file_url)
            response.raise_for_status()
            name = response.json()['name']
            download_url = '%s/download' % file_url

            if name not in cache:
                response = requests.get(download_url, stream=True)
                response.raise_for_status()
                cache.set(name, response.raw, read=True)

            yield (name, cache.get(name, read=True))

    return gen()

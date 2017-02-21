import diskcache
import requests
import tempfile
import os


response = requests.get(
    'https://data.kitware.com/api/v1/file/5893921d8d777f07219fca7e/download',
    stream=True)

url = 'https://data.kitware.com/api/v1/file'
_id = '5893921d8d777f07219fca7e'


# Get the test data
def test_image():
    download_url = '%s/%s/download' % (url, _id)
    cache_path = os.path.join(tempfile.gettempdir(), 'tomviz_test_cache')
    cache = diskcache.Cache(cache_path)

    if download_url not in cache:
        response = requests.get(download_url, stream=True)
        response.raise_for_status()
        cache.set(download_url, response.raw, read=True)

    return cache.get(download_url, read=True)

import pytest
import requests
import diskcache
import tempfile
import os
import tarfile
import shutil

DATA_URL = 'https://data.kitware.com/api/v1/file'


@pytest.fixture(scope="module")
def test_state_file(tmpdir_factory):
    tmpdir = tmpdir_factory.mktemp('state')

    _id = '5acbb5958d777f0685792521'
    download_url = '%s/%s/download' % (DATA_URL, _id)
    cache_path = os.path.join(tempfile.gettempdir(), 'tomviz_test_cache')
    with diskcache.Cache(cache_path) as cache:
        state_file_key = '%s#%s' % (download_url, 'state.tvsm')
        if download_url not in cache:
            response = requests.get(download_url, stream=True)
            response.raise_for_status()

            with tempfile.TemporaryFile() as fp:
                for chunk in response.iter_content(chunk_size=1024):
                    if chunk:
                        fp.write(chunk)

                fp.seek(0)

                with tarfile.open(fileobj=fp, mode="r:gz") as tar:
                    for member in tar.getmembers():
                        member_fp = tar.extractfile(member)
                        key = '%s#%s' % (download_url, member.name)
                        cache.set(key, member_fp, read=True)

        state_file_path = tmpdir.join('state.tvsm')
        with open(state_file_path.strpath, 'wb') as state_fp:
            shutil.copyfileobj(cache.get(state_file_key, read=True), state_fp)

        data_file_key = '%s#%s' % (download_url, 'small.emd')
        data_file_path = tmpdir.join('small.emd')
        with open(data_file_path.strpath, 'wb') as data_fp:
            shutil.copyfileobj(cache.get(data_file_key, read=True), data_fp)

        yield state_file_path

        tmpdir.remove()

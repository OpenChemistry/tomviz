#!/usr/bin/env python
import os
import hashlib
import requests
import sys
import tarfile

downloads = [{
    '_id': '59944e318d777f7d33e9c398',
    'name': 'paraview.tgz',
    'target': '.',
}, {
    '_id': '59944ea78d777f7d33e9c39d',
    'name': 'itk.tgz',
    'target': '.',
}, {
    '_id': '59944fce8d777f7d33e9c3a0',
    'name': 'tbb.tgz',
    'target': '.',
}, {
    '_id': '59944ffb8d777f7d33e9c3a3',
    'name': 'python.tgz',
    'target': '.',
}, {
    '_id': '599450538d777f7d33e9c3a6',
    'name': 'qt.tgz',
    'target': '.',
}, {
    '_id': '599451258d777f7d33e9c3a9',
    'name': 'googletest.tgz',
    'target': '.',
}]

girder_url = 'https://data.kitware.com/api/v1'
sha_dir = './sha512s'

def is_cached(id):
    sha_path = '%s/%s.sha512' % (sha_dir, download['name'])
    if os.path.exists(sha_path):
        url = '%s/file/%s/hashsum_file/sha512' % (girder_url, download['_id'])
        response = requests.get(url)
        sha = response.content

        with open(sha_path) as fp:
            cached_sha = fp.read()

        return sha == cached_sha

    return False


def extract(download):
    target = download['target']
    name = download['name']

    with tarfile.open(name) as tar:
        tar.extractall(path=target)


CHUNK_SIZE = 16 * 1024


def cache_download(download):
    url = '%s/file/%s/download' % (girder_url, download['_id'])
    response = requests.get(url, stream=True)

    sha = hashlib.sha512()
    name = download['name']
    print('Downloading %s ...' % name)
    with open(name, "wb") as fp:
        for chunk in response.iter_content(chunk_size=CHUNK_SIZE):
            if chunk:
                fp.write(chunk)
                sha.update(chunk)

    if not os.path.exists(sha_dir):
        os.makedirs(sha_dir)
    sha_path = '%s/%s.sha512' % (sha_dir, download['name'])
    print('Writing SHA512 to %s ...' % sha_path)

    with open(sha_path, 'w') as sha_fp:
        sha_fp.write(sha.hexdigest())


for download in downloads:

    if not is_cached(download['_id']):
        cache_download(download)
        extract(download)
    else:
        print('Using cache for %s' % download['name'])

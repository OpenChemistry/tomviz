#!/usr/bin/env python
import os
import hashlib
import requests
import subprocess
import sys

downloads = [{
    '_id': '598359e08d777f7d33e9c05b',
    'name': 'paraview.zip',
    'target': '.',
}, {
    '_id': '58a21ab58d777f0721a4b212',
    'name': 'itk-4.9.0-windows-64bit.zip',
    'target': 'itk',
}, {
    '_id': '58a220818d777f0721a4cc4a',
    'name': 'tbb.zip',
    'target': '.',
}, {
    '_id': '598239648d777f16d01ea145',
    'name': 'python-3.6.0.zip',
    'target': '.',
}, {
    '_id': '58a3436c8d777f0721a61036',
    'name': 'googletest-install.zip',
    'target': '.',
}, {
    '_id': '598b11ed8d777f7d33e9c1bf',
    'name': 'numpy.zip',
    'target': 'python\\bin\\Lib\\site-packages',
}]

girder_url = 'https://data.kitware.com/api/v1'


def is_cached(id):
    sha_path = '%s.sha512' % download['name']
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

    print('Extracting %s to %s ...' % (name, target))
    if not os.path.exists(target):
        os.makedirs(target)

    # Call out to 7z, zipfile seems to be very slow for this sort of network
    # filesystem.
    cmd = ['7z', 'x', name, '-o%s' % target]
    print('Extract command: %s' % cmd)
    process = subprocess.Popen(cmd)
    process.wait()
    if process.returncode != 0:
        sys.exit(process.returncode)


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

    sha_path = '%s.sha512' % download['name']
    print('Writing SHA512 to %s ...' % sha_path)

    with open(sha_path, 'w') as sha_fp:
        sha_fp.write(sha.hexdigest())


for download in downloads:

    if not is_cached(download['_id']):
        cache_download(download)
    else:
        print('Using cache for %s' % download['name'])

    extract(download)

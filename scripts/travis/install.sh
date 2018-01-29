#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
    # For acquisition testing
    sudo -H python -m ensurepip
    sudo -H pip install -U pip
    sudo -H pip install --ignore-installed -r $TRAVIS_BUILD_DIR/acquisition/requirements-dev.txt
    sudo chmod u+w ./python/lib/libpython3.6m.dylib
    ls -la ./python/lib/libpython3.6m.dylib
    $TRAVIS_BUILD_DIR/scripts/travis/download_deps.py
else
    pip install flake8
fi

#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
    # For acquisition testing
    brew update
    brew upgrade openssl
    brew upgrade python
    pip3 install --upgrade pip setuptools wheel
    pip3 install --ignore-installed -r $TRAVIS_BUILD_DIR/acquisition/requirements-dev.txt
    python3 $TRAVIS_BUILD_DIR/scripts/travis/download_deps.py
else
    pip install flake8
fi

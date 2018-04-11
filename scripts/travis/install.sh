#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
    # For acquisition testing
    brew update
    brew upgrade openssl
    brew upgrade python
    sudo chown -R "$USER" /usr/local
    pip3 install --upgrade pip setuptools wheel
    pip3 install --ignore-installed -r $TRAVIS_BUILD_DIR/acquisition/requirements-dev.txt
    pip3 install --ignore-installed -r $TRAVIS_BUILD_DIR/tests/python/requirements-dev.txt
    pip3 install --ignore-installed $TRAVIS_BUILD_DIR/tomviz/python/
    python3 $TRAVIS_BUILD_DIR/scripts/travis/download_deps.py
else
    pip install flake8
fi

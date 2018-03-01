#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
    # For acquisition testing
    brew update
    brew upgrade openssl
    brew upgrade python
    pip2 install --upgrade pip setuptools
    pip2 install --ignore-installed -r $TRAVIS_BUILD_DIR/acquisition/requirements-dev.txt
    python2 $TRAVIS_BUILD_DIR/scripts/travis/download_deps.py
else
    pip install flake8
fi

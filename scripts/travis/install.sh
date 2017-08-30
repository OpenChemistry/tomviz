#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
    # For acquisition testing
    pip install -r $TRAVIS_BUILD_DIR/acquisition/requirements-dev.txt
    $TRAVIS_BUILD_DIR/scripts/travis/download_deps.py
else
    pip install flake8
fi

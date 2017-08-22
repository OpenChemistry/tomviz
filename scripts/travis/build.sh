#!/bin/bash

if [[ $TRAVIS_OS_NAME == 'osx' ]]; then
    export DYLD_LIBRARY_PATH=/Users/travis/googletest-install/lib:$DYLD_LIBRARY_PATH
    export TOMVIZ_TEST_PYTHON_EXECUTABLE=/usr/local/bin/python
    ctest -VV -S $TRAVIS_BUILD_DIR/cmake/TravisContinuous.cmake
else
    cd $TRAVIS_BUILD_DIR
    if [ ${TRAVIS_PYTHON_VERSION:0:1} == "3" ]; then export PY3="true"; else export PY2="true"; fi
    if [ -n "${PY2}" ]; then ./scripts/travis/run_clang_format_diff.sh master $TRAVIS_COMMIT; fi
    git checkout $TRAVIS_PULL_REQUEST_SHA
    flake8 --config=flake8.cfg .
    cd "${TRAVIS_BUILD_DIR}/acquisition"
    pip install --upgrade pip setuptools wheel
    pip install --only-binary=numpy,scipy numpy scipy
    pip install -r requirements-dev.txt
    if [ -n "${PY2}" ]; then pytest -s; fi
    # Skip FEI test for Python 3
    if [ -n "${PY#}" ]; then pytest -s -k "not fei"; fi
fi

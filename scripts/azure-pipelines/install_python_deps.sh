#!/bin/bash

pip3 install --upgrade pip setuptools wheel
pip3 install numpy scipy
pip3 install -r acquisition/requirements-dev.txt
pip3 install -r tests/python/requirements-dev.txt
pip3 install acquisition

# We don't need pyfftw for the tests, it doesn't have wheels yet for
# python3.8, and it can be a pain to build, so skip installing it.
# Install the other dependencies manually.
pip3 install --no-deps tomviz/python
pip3 install tqdm h5py numpy click scipy itk

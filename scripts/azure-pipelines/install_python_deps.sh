#!/bin/bash

if [[ $AGENT_OS == 'Linux' ]]; then
  # h5py wheels for Linux python3.8 aren't on PyPI yet. Install from scipy.
  # Remove after this is fixed: https://github.com/h5py/h5py/issues/1410
  pip3 install http://wheels.scipy.org/h5py-2.10.0-cp38-cp38-manylinux1_x86_64.whl
elif [[ $AGENT_OS == 'Darwin' ]]; then
  # h5py wheels for Mac python3.8 aren't on PyPI yet. Install from scipy.
  # Remove after this is fixed: https://github.com/h5py/h5py/issues/1410
  pip3 install http://wheels.scipy.org/h5py-2.10.0-cp38-cp38-macosx_10_9_x86_64.whl
fi

pip3 install --upgrade pip setuptools wheel
pip3 install numpy scipy
pip3 install -r acquisition/requirements-dev.txt
pip3 install -r tests/python/requirements-dev.txt
pip3 install acquisition

# We don't need pyfftw for the tests, it doesn't have wheels yet for
# python3.8, and it can be a pain to build, so skip installing it.
# Install the other dependencies manually.
pip3 install --no-deps tomviz/python
pip3 install tqdm h5py numpy==1.16.4 click scipy itk

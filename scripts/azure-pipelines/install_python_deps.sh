#!/bin/bash

pip3 install --upgrade pip setuptools wheel
pip3 install numpy scipy
pip3 install -r acquisition/requirements-dev.txt
pip3 install -r tests/python/requirements-dev.txt
pip3 install tomviz/python
pip3 install acquisition

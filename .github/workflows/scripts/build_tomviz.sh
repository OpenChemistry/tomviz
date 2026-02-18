#!/usr/bin/env bash

mkdir -p tomviz-build && cd tomviz-build

# FIXME: setting the zlib paths manually shouldn't be necessary forever.
# Try removing it sometime
cmake -G"Ninja" -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
  -DCMAKE_INSTALL_LIBDIR:STRING=lib \
  -DTOMVIZ_USE_EXTERNAL_VTK:BOOL=ON \
  -DENABLE_TESTING:BOOL=ON \
  -DPython3_FIND_STRATEGY:STRING=LOCATION \
  -DZLIB_LIBRARY=$CONDA_PREFIX/lib/libz.so.1 \
  -DZLIB_INCLUDE_DIR=$CONDA_PREFIX/include \
  ../tomviz
ninja

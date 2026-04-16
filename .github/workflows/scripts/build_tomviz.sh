#!/usr/bin/env bash

mkdir -p tomviz-build && cd tomviz-build

CMAKE_ARGS=(
  -G"Ninja"
  -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
  -DCMAKE_INSTALL_LIBDIR:STRING=lib
  -DTOMVIZ_USE_EXTERNAL_VTK:BOOL=ON
  -DENABLE_TESTING:BOOL=ON
  -DPython3_FIND_STRATEGY:STRING=LOCATION
)

# FIXME: setting the zlib paths manually shouldn't be necessary forever.
# Try removing it sometime
case "$(uname -s)" in
  Linux*)
    CMAKE_ARGS+=(
      -DZLIB_LIBRARY=$CONDA_PREFIX/lib/libz.so.1
      -DZLIB_INCLUDE_DIR=$CONDA_PREFIX/include
    )
    ;;
  Darwin*)
    CMAKE_ARGS+=(
      -DZLIB_LIBRARY=$CONDA_PREFIX/lib/libz.dylib
      -DZLIB_INCLUDE_DIR=$CONDA_PREFIX/include
    )
    ;;
  MINGW*|MSYS*)
    CMAKE_ARGS+=(
      -DZLIB_LIBRARY=$CONDA_PREFIX/Library/lib/zlib.lib
      -DZLIB_INCLUDE_DIR=$CONDA_PREFIX/Library/include
    )
    ;;
esac

cmake "${CMAKE_ARGS[@]}" ../tomviz
ninja

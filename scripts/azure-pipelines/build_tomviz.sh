#!/bin/bash

if [[ $AGENT_OS == 'Darwin' ]]; then
  # CMake can't find Qt on mac os unless we do this
  CMAKE_EXTRA_ARGS="-DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5/ "
  # Resort to brute force to get CMake to find the right library!
  CMAKE_EXTRA_ARGS+="-DPython3_LIBRARY=$(dirname $(which python))/../lib/libpython3.8.dylib "
elif [[ $AGENT_OS == 'Windows_NT' ]]; then
  export CC=cl
  export CXX=cl
  CMAKE_EXTRA_ARGS="-DQt5_DIR:PATH=$PIPELINE_WORKSPACE\\Qt5.12.3_msvc2017_64\\lib\\cmake\\Qt5 "
  CMAKE_EXTRA_ARGS+="-DGTEST_LIBRARY:PATH=$PIPELINE_WORKSPACE\\googletest-install\\lib\\gtest.lib "
  CMAKE_EXTRA_ARGS+="-DGTEST_MAIN_LIBRARY:PATH=$PIPELINE_WORKSPACE\\googletest-install\\lib\\gtest_main.lib "
  CMAKE_EXTRA_ARGS+="-DGTEST_INCLUDE_DIR:PATH=$PIPELINE_WORKSPACE\\googletest-install\\include "
fi

if [[ $AGENT_OS == 'Windows_NT' ]]; then
  BUILD_TYPE='Release'
else
  BUILD_TYPE='RelWithDebInfo'
fi

# Get the location of paraview-config.cmake
PARAVIEW_DIR=$(find $PARAVIEW_INSTALL_FOLDER -name "paraview-config.cmake" -or -name "ParaViewConfig.cmake" | head -n 1 | xargs dirname)

cd $BUILD_BINARIESDIRECTORY

cmake $BUILD_SOURCESDIRECTORY \
  -DCMAKE_BUILD_TYPE:STRING=$BUILD_TYPE \
  -DENABLE_TESTING:BOOL=ON \
  -DParaView_DIR:PATH=$PARAVIEW_DIR \
  -DSKIP_PARAVIEW_ITK_PYTHON_CHECKS=ON \
  -DPython3_EXECUTABLE=$(which python) \
  $CMAKE_EXTRA_ARGS \
  -G Ninja

cmake --build .

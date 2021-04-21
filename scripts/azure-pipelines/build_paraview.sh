#!/bin/bash

if [[ $AGENT_OS == 'Darwin' ]]; then
  # CMake can't find Qt on mac os unless we do this
  CMAKE_EXTRA_ARGS="-DCMAKE_PREFIX_PATH=/usr/local/opt/qt@5/ "
elif [[ $AGENT_OS == 'Windows_NT' ]]; then
  export CC=cl
  export CXX=cl
  CMAKE_EXTRA_ARGS="-DQt5_DIR:PATH=$PIPELINE_WORKSPACE\\Qt5.12.3_msvc2017_64\\lib\\cmake\\Qt5"
fi

if [[ $AGENT_OS == 'Windows_NT' ]]; then
  BUILD_TYPE='Release'
else
  BUILD_TYPE='RelWithDebInfo'
fi

mkdir -p $PARAVIEW_BUILD_FOLDER
cd $PARAVIEW_BUILD_FOLDER

cmake $PARAVIEW_SOURCE_FOLDER \
  -DCMAKE_INSTALL_PREFIX=$PARAVIEW_INSTALL_FOLDER \
  -DCMAKE_BUILD_TYPE:STRING=$BUILD_TYPE \
  -DBUILD_TESTING:BOOL=OFF \
  -DPARAVIEW_USE_PYTHON:BOOL=ON \
  -DPARAVIEW_ENABLE_WEB:BOOL=OFF \
  -DPARAVIEW_ENABLE_EMBEDDED_DOCUMENTATION:BOOL=OFF \
  -DPARAVIEW_USE_QTHELP:BOOL=OFF \
  -DVTK_SMP_IMPLEMENTATION_TYPE:STRING=TBB \
  -DVTK_PYTHON_VERSION:STRING=3 \
  -DVTK_PYTHON_FULL_THREADSAFE:BOOL=ON \
  -DVTK_NO_PYTHON_THREADS:BOOL=OFF \
  -DPARAVIEW_USE_VTKM:BOOL=OFF \
  -DPARAVIEW_PLUGINS_DEFAULT:BOOL=OFF \
  -DPython3_EXECUTABLE=$(which python) \
  $CMAKE_EXTRA_ARGS \
  -G Ninja

cmake --build .
cmake --install .

# DIRTY: copy the tiffconf.h file to the install tree
# Otherwise, tomviz can't build against the install tree because
# tomviz includes "vtk_tiff.h".
TIFFCONF_PATH=$(find $PARAVIEW_BUILD_FOLDER -name "tiffconf.h" | head -n 1)
VTKTIFF_PATH=$(find $PARAVIEW_INSTALL_FOLDER -name "vtk_tiff.h" | head -n 1)
cp $TIFFCONF_PATH $(dirname $VTKTIFF_PATH)

Building Tomviz
===============

The Tomviz project reuses a number of components from Python, VTK, ITK, and
ParaView, so these dependencies will need to be compiled first in order to
build Tomviz. There is a superbuild that automates most of this when building
binaries. We recommend using these binaries where available if you wish to
test, and nightly binaries are made available in addition to releases.

If you wish to develop Tomviz, the following sequence of commands will build
a suitable ParaView, ITK, and a Tomviz that uses these builds. You should use
your package manager/installers/upstream build instructions to install the
dependencies. CMake will search for these, and the cmake-gui can be used to
point to them if they are not found automatically.

Dependencies
------------

 * Qt 5.6.0 (5.9.1 recommended)
 * CMake 3.3
 * Python 3.6
 * NumPy 1.12
 * Git 2.1
 * C++ compiler with C++11 support
 * Intel TBB

Initial Build
-------------

You will need the development headers, so please ensure they are installed
(several distros use -dev packages not installed with the main package). If
you are in a directory where you would like to place the source and builds
with all prerequisites installed:

    git clone --recursive git://github.com/kitware/paraview.git
    git clone --recursive git://github.com/openchemistry/tomviz.git
    mkdir paraview-build
    cd paraview-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DBUILD_TESTING:BOOL=OFF \
      -DPARAVIEW_ENABLE_CATALYST:BOOL=OFF \
      -DPARAVIEW_ENABLE_PYTHON:BOOL=ON \
      -DPARAVIEW_ENABLE_WEB:BOOL=OFF \
      -DPARAVIEW_ENABLE_EMBEDDED_DOCUMENTATION:BOOL=OFF\
      -DPARAVIEW_USE_QTHELP:BOOL=OFF \
      -DVTK_SMP_IMPLEMENTATION_TYPE:STRING=TBB \
      -DVTK_PYTHON_VERSION:STRING=3 \
      -DVTK_PYTHON_FULL_THREADSAFE:BOOL=ON \
      -DVTK_NO_PYTHON_THREADS:BOOL=OFF \
      ../paraview
    #****
    cmake --build .

This will clone all source code needed, configure and build a minimal ParaView,
the Ninja generator is recommended to build faster, but optional, on Windows
you will need to specify the correct generator for the installed compiler.

Note: since the switch to Python 3 it is highly recommended to stop at the point
marked with `#****` and check which version of Python CMake found.  Make sure the
`PYTHON_EXECUTABLE` `PYTHON_INCLUDE_DIR` and `PYTHON_LIBRARY` variables are pointing
to the python you want to use.  Compiling against Anaconda packages is **not**
supported.  If you know your python is installed in /usr/local (the default for Homebrew)
you can run `find /usr/local -name patchlevel.h` to find the value for `PYTHON_INCLUDE_DIR`
and `find /usr/local -name libpython3.6m.*` to find the value for `PYTHON_LIBRARY`.

    cd ..
    git clone git://itk.org/ITK.git
    cd ITK
    git checkout v4.12.0
    cd ..
    mkdir itk-build
    cd itk-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DITK_LEGACY_REMOVE:BOOL=ON \
      -DITK_LEGACY_SILENT:BOOL=ON \
      -DITK_USE_FFTWF:BOOL=ON \
      -DModule_BridgeNumPy:BOOL=ON \
      -DBUILD_TESTING:BOOL=OFF \
      -DITK_WRAP_unsigned_short:BOOL=ON \
      -DITK_WRAP_rgb_unsigned_char:BOOL=OFF \
      -DITK_WRAP_rgba_unsigned_char:BOOL=OFF \
      -DITK_BUILD_DEFAULT_MODULES:BOOL=OFF \
      -DITKGroup_Core:BOOL=ON \
      -DITKGroup_Filtering:BOOL=ON \
      -DITKGroup_Segmentation:BOOL=ON \
      -DITKGroup_Registration:BOOL=ON \
      -DITKGroup_Nonunit:BOOL=ON \
      -DPython_ADDITIONAL_VERSIONS:STRING=3 \
      -DITK_WRAP_PYTHON:BOOL=ON \
      -DBUILD_EXAMPLES:BOOL=OFF \
      -DBUILD_SHARED_LIBS:BOOL=ON \
      ../ITK
    cmake --build .

This will build ITK with the its Python wrapping turned on, you must inspect
the CMake configuration to ensure it uses the same Python as ParaView. ITK,
while not required to build Tomviz, is required for any of the ITK-based data
operators to run.

Now, to build Tomviz:

    cd ..
    export build_root=`pwd`
    mkdir tomviz-build
    cd tomviz-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DParaView_DIR:PATH=$build_root/paraview-build \
      -DITK_DIR:PATH=$build_root/itk-build \
      ../tomviz
    cmake --build .

Running Tomviz
--------------

You should be able to simply execute the binary from the build tree, on Windows
this is a little more difficult as you must ensure all DLLs are in the same
directory or in your 'PATH' environment variable.

    ./bin/tomviz

Updating Source and Rebuilding
------------------------------

Once this completes you will have a binary in the 'bin' directory of your build
tree. It is possible to update the ParaView and/or Tomviz source directories
and rebuild the binaries incrementally. The following would update both source
trees and rebuild the latest version of each:

    cd ../paraview
    git pull && git submodule update
    cd ../paraview-build
    cmake --build .
    cd ../tomviz
    git pull && git submodule update
    cd ../tomviz-build
    cmake --build .

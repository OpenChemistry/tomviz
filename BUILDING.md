Building tomviz
===============

The tomviz project reuses a number of components from Python, VTK, and
ParaView, so these dependencies will need to be compiled first in order to
build tomviz. There is a superbuild that automates most of this when building
binaries. We recommend using these binaries where available if you wish to
test, and nightly binaries are made available in addition to releases.

If you wish to develop tomviz, the following sequence of commands will build
a suitable ParaView, and a tomviz that uses the ParaView just compiled. You
need to use your package manager/installers/upstream build instructions to
install out dependencies. CMake will search for these, and the cmake-gui can
be used to point to them if they are not found automatically.

Dependencies
------------

 * Qt 4.8
 * CMake 3.1 (2.8.8 minimum)
 * Python 2.7
 * NumPy 1.8
 * Git 2.1
 * C++ compiler

Initial Build
-------------

You will need the development headers, so please ensure these are installed
(several distros use -dev packages not installed with the main package). If
you are in a directory you would like to place the source and builds with all
prerequisites installed:

    git clone --recursive git://github.com/kitware/paraview.git
    git clone git://github.com/openchemistry/tomviz.git
    mkdir paraview-build
    cd paraview-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DPARAVIEW_ENABLE_CATALYST:BOOL=OFF -DPARAVIEW_ENABLE_PYTHON:BOOL=ON \
      -DVTK_RENDERING_BACKEND:STRING=OpenGL2 -DPARAVIEW_ENABLE_WEB:BOOL=OFF \
      ../paraview
    cmake --build .

This will clone all source code needed, configure and build a minimal ParaView,
the Ninja generator is recommended to build faster, but optional, on Windows
you will need to specify the correct generator for the installed compiler. Now,
to build tomviz:

    cd ..
    mkdir tomviz-build
    cd tomviz-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DParaView_DIR:PATH=../paraview-build ../tomviz
    cmake --build .

Running tomviz
--------------

You should be able to simply execute the binary from the build tree, on Windows
this is a little more difficult as you must ensure all DLLs are in the same
directory or in your 'PATH' environment variable.

    ./bin/tomviz

Updating Source and Rebuilding
------------------------------

Once this completes you will have a binary in the 'bin' directory of your build
tree. It is possible to update the ParaView and/or tomviz source directories
and rebuild the binaries incrementally. The following would update both source
trees and rebuild the latest version of each:

    cd ../paraview
    git pull && git submodule update
    cd ../paraview-build
    cmake --build .
    cd ../tomviz
    git pull
    cd ../tomviz-build
    cmake --build .

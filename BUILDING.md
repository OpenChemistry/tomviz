Building Tomviz
===============

The Tomviz project reuses a number of components from Python, VTK, ITK, and
ParaView, so these dependencies will need to be compiled first in order to
build Tomviz. There is a superbuild that automates most of this when building
binaries. We recommend using these binaries where available if you wish to
test, and nightly binaries are made available in addition to releases.

To develop Tomviz we recommend using our developer [superbuild][super]. That
repository will clone this one, and several others such as the correct ParaView
version and optionally the ITK release needed. It will build them all with the
correct flags, and ensure they link to the correct versions of projects. Going
forward this is our preferred development build environment, enabling a quick
and easy set up. The use of git submodules ensures you will not lose any changes
and will have full access to the source code. You should commit changes from the
Tomviz repository contained there.

Advanced Build Instructions
---------------------------

These are retained for historical reasons and for the brave. Most of these steps
are contained and automated in the `tomvizsuper` repository, and that also tries
to ensure that the same Qt, Python, etc are used in the contained projects. This
is how you pass the flags circa late 2019, and in early 2020 we decided to
remove some automated build consistency checks that mean developers must work
harder to ensure build consistency with respect to Qt, Python and other third
party libraries.

If you wish to develop Tomviz, the following sequence of commands will build
a suitable ParaView, ITK, and a Tomviz that uses these builds. You should use
your package manager/installers/upstream build instructions to install the
dependencies. CMake will search for these, and the cmake-gui can be used to
point to them if they are not found automatically.

Dependencies
------------

 * Qt 5.9+ (5.9 recommended)
 * CMake 3.3+
 * Python 3.7
 * NumPy 1.12
 * Git 2.1
 * C++ compiler with C++11 support (MSVC 2015 on Windows)
 * Intel TBB

Initial Build
-------------

You will need the development headers, so please ensure they are installed
(several distros use -dev packages not installed with the main package). If
you are in a directory where you would like to place the source and builds
with all prerequisites installed:

    git clone --recursive git://github.com/openchemistry/paraview.git
    mkdir paraview-build
    cd paraview-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DBUILD_TESTING:BOOL=OFF \
      -DPARAVIEW_ENABLE_CATALYST:BOOL=OFF \
      -DPARAVIEW_ENABLE_PYTHON:BOOL=ON \
      -DPARAVIEW_ENABLE_WEB:BOOL=OFF \
      -DPARAVIEW_ENABLE_EMBEDDED_DOCUMENTATION:BOOL=OFF\
      -DPARAVIEW_USE_QTHELP:BOOL=OFF \
      -DPARAVIEW_PLUGINS_DEFAULT:BOOL=OFF \
      -DVTK_SMP_IMPLEMENTATION_TYPE:STRING=TBB \
      -DVTK_PYTHON_VERSION:STRING=3 \
      -DVTK_PYTHON_FULL_THREADSAFE:BOOL=ON \
      -DVTK_NO_PYTHON_THREADS:BOOL=OFF \
      ../paraview

Check which version of Python CMake found. Ensure the `PYTHON_EXECUTABLE`,
`PYTHON_INCLUDE_DIR`, and `PYTHON_LIBRARY` variables are pointing to a
consistent version of Python in the same prefix. Compiling against Anaconda
packages is **not** recommended. If they are consistent proceed to build
ParaView:

    cmake --build .

This will clone all source code needed, configure and build a minimal ParaView,
the Ninja generator is recommended for faster builds, but optional. On Windows
you will need to specify the correct generator for the installed compiler.

    cd ..
    git clone git://itk.org/ITK.git
    cd ITK
    git checkout v5.0.1
    cd ..
    mkdir itk-build
    cd itk-build
    cmake -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo \
      -DITK_LEGACY_REMOVE:BOOL=ON \
      -DITK_LEGACY_SILENT:BOOL=ON \
      -DModule_ITKBridgeNumPy:BOOL=ON \
      -DBUILD_TESTING:BOOL=OFF \
      -DITK_WRAP_unsigned_short:BOOL=ON \
      -DITK_WRAP_rgb_unsigned_char:BOOL=OFF \
      -DITK_WRAP_rgba_unsigned_char:BOOL=OFF \
      -DITK_BUILD_DEFAULT_MODULES:BOOL=OFF \
      -DITKGroup_Core:BOOL=ON \
      -DITKGroup_Filtering:BOOL=ON \
      -DITKGroup_Segmentation:BOOL=ON \
      -DITKGroup_Nonunit:BOOL=ON \
      -DPython_ADDITIONAL_VERSIONS:STRING=3 \
      -DITK_WRAP_PYTHON:BOOL=ON \
      -DBUILD_EXAMPLES:BOOL=OFF \
      -DBUILD_SHARED_LIBS:BOOL=ON \
      ../ITK
    cmake --build .

This will build ITK with its Python wrapping turned on, you must inspect
the CMake configuration to ensure it uses the same Python as ParaView. ITK,
while not required to build Tomviz, is required for any of the ITK-based data
operators to run.

Now, to build Tomviz:

    cd ..
    export build_root=`pwd`
    git clone --recursive git://github.com/openchemistry/tomviz.git
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
    git pull && git submodule update --recursive
    cd ../paraview-build
    cmake --build .
    cd ../tomviz
    git pull && git submodule update
    cd ../tomviz-build
    cmake --build .


[super]: https://github.com/OpenChemistry/tomvizsuper

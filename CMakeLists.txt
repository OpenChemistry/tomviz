cmake_minimum_required(VERSION 3.3)
project(tomviz)

set(CMAKE_MODULE_PATH "${tomviz_SOURCE_DIR}/cmake")

# Request C++11 standard, using new CMake variables.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

include(BuildType)
include(BuildLocation)
include(CompilerFlags)
include(tomvizDetermineVersion)
include(Git)

determine_version("${tomviz_SOURCE_DIR}" "${GIT_EXECUTABLE}" "tomviz")
# Hard coded for source tarballs, releases, etc.
set(tomviz_version_major 0)
set(tomviz_version_minor 9)
set(tomviz_version_patch 1)
set(tomviz_version_extra)
set(tomviz_version
  "${tomviz_version_major}.${tomviz_version_minor}.${tomviz_version_patch}")
if(tomviz_VERSION)
  set(tomviz_version_major ${tomviz_VERSION_MAJOR})
  set(tomviz_version_minor ${tomviz_VERSION_MINOR})
  set(tomviz_version_patch ${tomviz_VERSION_PATCH})
  set(tomviz_version_extra ${tomviz_VERSION_PATCH_EXTRA})
endif()
set(tomviz_version
  "${tomviz_version_major}.${tomviz_version_minor}.${tomviz_version_patch}")

# Location where python modules will be installed.
set(tomviz_python_install_dir "lib/tomviz/site-packages")
if(APPLE)
  set(tomviz_python_install_dir "Applications/tomviz.app/Contents/Python")
endif()

# Location where python modules will be copied to in binary tree.
set(tomviz_python_binary_dir "${tomviz_BINARY_DIR}/lib/site-packages")

# Location where sample data will be installed.
set(tomviz_data_install_dir "share/tomviz")
if(APPLE)
  set(tomviz_data_install_dir "Applications/tomviz.app/Contents/share/tomviz")
endif()

set(PYBIND11_PYTHON_VERSION "2.7")
add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/pybind11)

# These dependencies are inherited from ParaView, we will ideally trim away
# Help and UiTools in the future (at least UiTools).
find_package(Qt5 REQUIRED COMPONENTS Help Network Widgets UiTools)
find_package(ParaView REQUIRED)

if(NOT PARAVIEW_BUILD_QT_GUI)
  message(FATAL_ERROR
    "Tomviz requires PARAVIEW_BUILD_QT_GUI to be enabled. "
    "Please rebuild ParaView with PARAVIEW_BUILD_QT_GUI set to TRUE.")
endif()
if(NOT PARAVIEW_ENABLE_PYTHON)
  message(FATAL_ERROR
    "Tomviz requires PARAVIEW_ENABLE_PYTHON to be enabled. "
    "Please rebuild ParaView with PARAVIEW_ENABLE_PYTHON set to TRUE.")
endif()
if(NOT PARAVIEW_QT_VERSION STREQUAL "5")
  message(FATAL_ERROR
    "Tomviz requires PARAVIEW_QT_VERSION to be 5, please rebuild ParaView.")
endif()

find_package(ITK 4.9)
if(ITK_FOUND AND NOT ITK_WRAP_PYTHON)
  message(FATAL_ERROR
    "Tomviz requires ITK_WRAP_PYTHON to be enabled. "
    "Please rebuild ITK with ITK_WRAP_PYTHON set to TRUE.")
endif()

add_subdirectory(tomviz)

option(ENABLE_TESTING "Enable testing and building the tests." OFF)
if(ENABLE_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

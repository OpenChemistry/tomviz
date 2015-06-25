cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(tomviz)

set(CMAKE_MODULE_PATH "${tomviz_SOURCE_DIR}/cmake")

include(BuildType)
include(BuildLocation)
include(CompilerFlags)

set (tomviz_version_major 0)
set (tomviz_version_minor 5)
set (tomviz_version_patch 0)
set (tomviz_version_suffix)
set (tomviz_version "${tomviz_version_major}.${tomviz_version_minor}.${tomviz_version_patch}")

# Location where python modules will be installed.
set(tomviz_python_install_dir "lib/tomviz-${tomviz_version}/site-packages")
if(APPLE)
  set(tomviz_python_install_dir "Applications/tomviz.app/Contents/Python")
endif()

# Location where sample data will be installed.
set(tomviz_data_install_dir "share/tomviz")
if(APPLE)
  set(tomviz_data_install_dir "Applications/tomviz.app/Contents/share/tomviz")
endif()

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

include_directories(SYSTEM "${PARAVIEW_INCLUDE_DIRS}")

find_package(Qt4 REQUIRED)
include_directories(SYSTEM "${QT_INCLUDES}")
add_definitions(${QT_DEFINITIONS})

add_subdirectory(tomviz)

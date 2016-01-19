cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
project(tomviz)

set(CMAKE_MODULE_PATH "${tomviz_SOURCE_DIR}/cmake")

include(BuildType)
include(BuildLocation)
include(CompilerFlags)
include(tomvizDetermineVersion)
include(Git)

determine_version("${tomviz_SOURCE_DIR}" "${GIT_EXECUTABLE}" "tomviz")
# Hard coded for source tarballs, releases, etc.
set(tomviz_version_major 0)
set(tomviz_version_minor 7)
set(tomviz_version_patch 2)
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

# Location where sample data will be installed.
set(tomviz_data_install_dir "share/tomviz")
if(APPLE)
  set(tomviz_data_install_dir "Applications/tomviz.app/Contents/share/tomviz")
endif()

find_package(ITK 4.9)
if(ITK-FOUND AND NOT ITK_WRAP_PYTHON)
  message(FATAL_ERROR
    "Tomviz requires ITK_WRAP_PYTHON to be enabled. "
    "Please rebuild ITK with ITK_WRAP_PYTHON set to TRUE.")
endif()

add_subdirectory(tomviz)

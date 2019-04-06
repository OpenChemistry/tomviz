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
set(tomviz_version_major 1)
set(tomviz_version_minor 6)
set(tomviz_version_patch 0)
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

# These dependencies are inherited from ParaView.
find_package(Qt5 REQUIRED COMPONENTS Concurrent Network Widgets)
find_package(ParaView REQUIRED)

# Use automoc, autouic, and autorcc for our Qt code.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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

load_cache(${ParaView_DIR}
  PARAVIEW_PYTHON_VERSION
)

if(NOT "${PARAVIEW_PYTHON_VERSION}")
  message(FATAL_ERROR "Failed to get PARAVIEW_PYTHON_VERSION")
endif()

# Get the ParaView Python variables
load_cache(${ParaView_DIR}
  READ_WITH_PREFIX Python${PARAVIEW_PYTHON_VERSION}_
  EXECUTABLE INCLUDE_DIR LIBRARY_RELEASE
)
set(ParaView_PYTHON_EXECUTABLE
    "${Python${PARAVIEW_PYTHON_VERSION}_EXECUTABLE}")
set(ParaView_PYTHON_INCLUDE_DIR
    "${Python${PARAVIEW_PYTHON_VERSION}_INCLUDE_DIR}")
set(ParaView_PYTHON_LIBRARY
    "${Python${PARAVIEW_PYTHON_VERSION}_LIBRARY_RELEASE}")

# Most of the time we don't want to skip these checks, default to off.
option(SKIP_PARAVIEW_ITK_PYTHON_CHECKS
  "Run checks to ensure ParaView and ITK are build against the same Python"
  OFF)
mark_as_advanced(SKIP_PARAVIEW_ITK_PYTHON_CHECKS)

find_package(ITK 4.9)
if(ITK_FOUND AND NOT SKIP_PARAVIEW_ITK_PYTHON_CHECKS)
  message(STATUS "Checking ParaView and ITK were built against the same Python")
  if(NOT ITK_WRAP_PYTHON)
    message(FATAL_ERROR
      "Tomviz requires ITK_WRAP_PYTHON to be enabled. "
      "Please rebuild ITK with ITK_WRAP_PYTHON set to TRUE.")
  endif()

  # Verify that the ITK build is using the same Python as the ParaView build
  load_cache(${ITK_DIR}
    READ_WITH_PREFIX ITK_
    PYTHON_INCLUDE_DIR PYTHON_LIBRARY
  )

  if (NOT ITK_PYTHON_INCLUDE_DIR STREQUAL ParaView_PYTHON_INCLUDE_DIR)
    message(FATAL_ERROR
      "The Python include directory is not the same for both ParaView and ITK. "
      "Please ensure ParaView and ITK are being built against the same Python. "
      "ParaView Python include directory: ${ParaView_PYTHON_INCLUDE_DIR}, "
      "ITK Python include directory: ${ITK_PYTHON_INCLUDE_DIR}"
    )
  endif()

  if (NOT ITK_PYTHON_LIBRARY STREQUAL ParaView_PYTHON_LIBRARY)
    message(FATAL_ERROR
      "The Python library is not the same for both ParaView and ITK. "
      "Please ensure ParaView and ITK are being built against the same Python. "
      "ParaView Python library: ${ParaView_PYTHON_LIBRARY}, "
      "ITK Python include directory: ${ITK_PYTHON_LIBRARY}"
    )
  endif()
endif()

# Ensure we use the same Python as ParaView
set(PYTHON_INCLUDE_DIR ${ParaView_PYTHON_INCLUDE_DIR} CACHE PATH "Tomviz")
set(PYTHON_LIBRARY ${ParaView_PYTHON_LIBRARY} CACHE PATH "Tomviz")
set(PYTHON_EXECUTABLE ${ParaView_PYTHON_EXECUTABLE} CACHE PATH "Tomviz")

set(PYBIND11_CPP_STANDARD "-std=c++11" CACHE STRING "")
add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/pybind11)

add_subdirectory(tomviz)

option(ENABLE_TESTING "Enable testing and building the tests." OFF)
if(ENABLE_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

# -----------------------------------------------------------------------------
# Add web application
# -----------------------------------------------------------------------------

option(TOMVIZ_DOWNLOAD_WEB  "Enable downloading web application." OFF)

set(tomviz_html_binary_dir "${CMAKE_BINARY_DIR}/web")
set(tomviz_js_binary_path "${tomviz_html_binary_dir}/tomviz.js")
set(tomviz_html_binary_path "${tomviz_html_binary_dir}/tomviz.html")

# -----------------------------------------------------------------------------
# Section to update when a new version of tomvizweb became available
# -----------------------------------------------------------------------------

set(tomvizweb_version "1.0.7")
set(tomviz_js_sha512 "820c4f4ed80044cd7f8cfcdf53f5fb961c5098a315bb208d9a0ff8bb94dc7e1c7aaa8f814390672c730546ec19c07d243d6846b8aba01e51625cf902d2b1eba0")
set(tomviz_html_sha512 "97d41012baf6300002cd59a6122b1fcedc75014d3ee876fcc9250ce9a6352b00495103473ee9ec28550c275a0038f98b1396306366609197d9901fda4b5d94cd")

# -----------------------------------------------------------------------------

if (TOMVIZ_DOWNLOAD_WEB)
  message(STATUS "Downloading tomviz.js")
  file(DOWNLOAD
    "https://unpkg.com/tomvizweb@${tomvizweb_version}"
    "${tomviz_js_binary_path}"
    EXPECTED_HASH SHA512=${tomviz_js_sha512}
    SHOW_PROGRESS
  )

  message(STATUS "Downloading tomviz.html")
  file(DOWNLOAD
    "https://unpkg.com/tomvizweb@${tomvizweb_version}/Distribution/template.html"
    "${tomviz_html_binary_path}"
    EXPECTED_HASH SHA512=${tomviz_html_sha512}
    SHOW_PROGRESS
  )
endif()

set(tomviz_web_install_dir "share/tomviz/web")
if(APPLE)
  set(tomviz_web_install_dir "Applications/tomviz.app/Contents/share/tomviz/web")
endif()

if(EXISTS "${tomviz_html_binary_path}")
  install(
    FILES "${tomviz_html_binary_path}"
    DESTINATION "${tomviz_web_install_dir}"
    COMPONENT runtime)
endif()

if(EXISTS "${tomviz_js_binary_path}")
  install(
    FILES "${tomviz_js_binary_path}"
    DESTINATION "${tomviz_web_install_dir}"
    COMPONENT runtime)
endif()

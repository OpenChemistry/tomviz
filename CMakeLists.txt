cmake_minimum_required(VERSION 3.3)
project(tomviz)

include(GNUInstallDirs)
if(NOT INSTALL_RUNTIME_DIR)
  set(INSTALL_RUNTIME_DIR "${CMAKE_INSTALL_BINDIR}")
endif()
if(NOT INSTALL_LIBRARY_DIR)
  set(INSTALL_LIBRARY_DIR "${CMAKE_INSTALL_LIBDIR}")
endif()
if(NOT INSTALL_ARCHIVE_DIR)
  set(INSTALL_ARCHIVE_DIR "${CMAKE_INSTALL_LIBDIR}")
endif()
if(NOT INSTALL_INCLUDE_DIR)
  set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_INCLUDEDIR}")
endif()
if(NOT INSTALL_DATA_DIR)
  set(INSTALL_DATA_DIR "${CMAKE_INSTALL_DATAROOTDIR}")
endif()
if(NOT INSTALL_DOC_DIR)
  set(INSTALL_DOC_DIR "${CMAKE_INSTALL_DOCDIR}")
endif()
if(NOT INSTALL_MAN_DIR)
  set(INSTALL_MAN_DIR "${CMAKE_INSTALL_MANDIR}")
endif()
if(UNIX AND NOT APPLE)
  if(NOT INSTALL_XDG_APP_DIR)
    set(INSTALL_XDG_APPS_DIR "${INSTALL_DATA_DIR}/applications")
  endif()
  if(NOT INSTALL_XDG_ICON_DIR)
    set(INSTALL_XDG_ICON_DIR "${INSTALL_DATA_DIR}/pixmaps")
  endif()
endif()


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
set(tomviz_version_minor 8)
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

find_package(ITK 4.9)

# Many things changed in the find Python logic and ParaView's willingness to
# share what version of Python it built against. In the end it wasn't worth
# trying to support doing this automatically - try to feed everything the same
# version of Python, using a superbuild will make this easier for you.
find_package(Python3 COMPONENTS Interpreter Development)

# Just add these to the cache so we can check them more easily...
set(PYTHON_INCLUDE_DIR ${PYTHON3_INCLUDE_DIR} CACHE PATH "Tomviz")
set(PYTHON_LIBRARY ${PYTHON3_LIBRARY} CACHE PATH "Tomviz")
set(PYTHON_EXECUTABLE ${PYTHON3_EXECUTABLE} CACHE PATH "Tomviz")

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

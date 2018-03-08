include(CxxTests.cmake)

find_package(GTest REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Test)

include_directories(SYSTEM
  ${QtCore_INCLUDE_DIRS}
  ${Qt5Test_INCLUDE_DIRS}
  ${PARAVIEW_INCLUDE_DIRS}
  ${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/tomviz)
include_directories(${PROJECT_SOURCE_DIR}/tomviz/acquisition)

include(CheckIncludeFileCXX)
include(CheckCXXSymbolExists)
check_include_file_cxx("gtest/gtest.h" HAVE_GTEST_HPP)
if(HAVE_GTEST_HPP)
  check_cxx_symbol_exists(GTEST_HAS_PTHREAD "gtest/gtest.h" GTEST_HAS_PTHREAD)
  check_cxx_symbol_exists(GTEST_IS_THREADSAFE "gtest/gtest.h" GTEST_IS_THREADSAFE)
endif()

if(GTEST_HAS_PTHREAD)
  message(STATUS "GTest claims it has pthreads, we need to link to it.")
  find_package(Threads)
  set(EXTRA_LINK_LIB ${CMAKE_THREAD_LIBS_INIT})
else()
  set(EXTRA_LINK_LIB "")
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/TomvizTest.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/TomvizTest.h" @ONLY)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

if(WIN32)
  set(_separator "\\;")
else()
  set(_separator ":")
endif()

set(_pythonpath "${tomviz_python_binary_dir}${_separator}")
set(_pythonpath "${_pythonpath}${_separator}${PROJECT_BINARY_DIR}/lib")
set(_pythonpath "${_pythonpath}${_separator}${ParaView_DIR}/lib/site-packages")
set(_pythonpath "${_pythonpath}${_separator}${ParaView_DIR}/lib")
set(_pythonpath "${_pythonpath}${_separator}${ITK_DIR}/lib")
set(_pythonpath "${_pythonpath}${_separator}${ITK_DIR}/Wrapping/Generators/Python")
set(_pythonpath "${_pythonpath}${_separator}$ENV{PYTHONPATH}")

# Add the test cases
add_cxx_test(OperatorPython PYTHONPATH ${_pythonpath})
add_cxx_test(Variant)

add_cxx_qtest(AcquisitionClient PYTHONPATH "${CMAKE_SOURCE_DIR}/acquisition")


# Generate the executable
create_test_executable(tomvizTests)

target_link_libraries(tomvizTests Qt5::Test)

set(CTEST_SOURCE_DIRECTORY "$ENV{TRAVIS_BUILD_DIR}")
set(CTEST_BINARY_DIRECTORY "/Users/travis/tomviz-build")
# We need to set this otherwise we yet 255 as our return code!
set(CTEST_COMMAND ctest)
include(${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake)
set(CTEST_SITE "Travis")
set(_prefix "master")
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
set(CTEST_BUILD_NAME "${_prefix}-#$ENV{TRAVIS_BUILD_NUMBER}")
set(cfg_options
  -DQt5_DIR:PATH=/Users/travis/qt-5.9.1/lib/cmake/Qt5
  -DParaView_DIR:PATH=/Users/travis/paraview-build/
  -DITK_DIR:PATH=/Users/travis/itk-install/lib/cmake/ITK-4.13
  -DGTEST_LIBRARY:PATH=/Users/travis/googletest-install/lib/libgtest.dylib
  -DGTEST_MAIN_LIBRARY:PATH=/Users/travis/googletest-install/lib/libgtest_main.dylib
  -DGTEST_INCLUDE_DIR:PATH=/Users/travis/googletest-install/include
  -DPYTHON_EXECUTABLE:PATH=/Users/travis/python/bin/python3.6
  -DENABLE_TESTING:BOOL=ON
  -DSKIP_PARAVIEW_ITK_PYTHON_CHECKS:BOOL=ON
  -DCMAKE_BUILD_TYPE:STRING="RelWithDebInfo"
  -DPYBIND11_PYTHON_VERSION:STRING=3.6)

ctest_start("Continuous")
ctest_configure(OPTIONS "${cfg_options}")
ctest_build()
ctest_test(RETURN_VALUE rv)
ctest_submit()

if(NOT rv EQUAL 0)
  message(FATAL_ERROR "Test failures occurred.")
endif()

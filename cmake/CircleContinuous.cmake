set(CTEST_SOURCE_DIRECTORY "/tomviz")
set(CTEST_BINARY_DIRECTORY "/tomviz-build")
# We need to set this otherwise we yet 255 as our return code!
set(CTEST_COMMAND ctest)
include(${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake)
set(CTEST_SITE "CircleCI")
set(CTEST_BUILD_NAME "$ENV{CIRCLE_BRANCH}-#$ENV{CIRCLE_BUILD_NUM}")
set(CTEST_CMAKE_GENERATOR "Ninja")
set(cfg_options
  -DCMAKE_BUILD_TYPE:STRING=Release
  -DENABLE_TESTING:BOOL=ON
  -DParaView_DIR:PATH=$ENV{PARAVIEW_DIR}
  -DITK_DIR:PATH=$ENV{ITK_DIR}
  -DQt5_DIR:PATH=$ENV{QT5_DIR}
  -DGTEST_LIBRARY:PATH=$ENV{GTEST_LIBRARY}
  -DGTEST_MAIN_LIBRARY:PATH=$ENV{GTEST_MAIN_LIBRARY}
  -DGTEST_INCLUDE_DIR:PATH=$ENV{GTEST_INCLUDE_DIR})

ctest_start("Continuous")
ctest_configure(OPTIONS "${cfg_options}")
ctest_build()
ctest_test(RETURN_VALUE rv)
ctest_submit()

if(NOT rv EQUAL 0)
  message(FATAL_ERROR "Test failures occurred.")
endif()

set(CTEST_SOURCE_DIRECTORY "$ENV{APPVEYOR_BUILD_FOLDER}")
set(CTEST_BINARY_DIRECTORY "C:/projects/tomviz-build")
# We need to set this otherwise we yet 255 as our return code!
set(CTEST_COMMAND ctest)
include(${CTEST_SOURCE_DIRECTORY}/CTestConfig.cmake)
set(CTEST_SITE "AppVeyor")
string(SUBSTRING "$ENV{APPVEYOR_REPO_COMMIT}" 0 8 _sha)
set(CTEST_BUILD_NAME "${_sha}-#$ENV{APPVEYOR_BUILD_NUMBER}")
set(CTEST_CMAKE_GENERATOR "Visual Studio 14 2015 Win64")
set(CTEST_CONFIGURATION_TYPE "Release")
set(cfg_options
  -DQt5_DIR:PATH=C:/Qt/5.7/msvc2015_64/lib/cmake/Qt5
  -DParaView_DIR:PATH=C:/projects/paraview-install/lib/cmake/paraview-5.2
  -DITK_DIR:PATH=C:/projects/itk/lib/cmake/ITK-4.9
  -DGTEST_LIBRARY:PATH=C:/projects/googletest-install/lib/gtest.lib
  -DGTEST_MAIN_LIBRARY:PATH=C:/projects/googletest-install/lib/gtest_main.lib
  -DGTEST_INCLUDE_DIR:PATH=C:/projects/googletest-install/include
  -DPYTHON_EXECUTABLE:PATH=C:/projects/python/bin/python.exe
  -DENABLE_TESTING:BOOL=ON
  -DCMAKE_BUILD_TYPE:STRING="Release")

ctest_start("Continuous")
ctest_configure(OPTIONS "${cfg_options}")
ctest_build()
ctest_test(RETURN_VALUE rv)
ctest_submit()

if(NOT rv EQUAL 0)
  message(FATAL_ERROR "Test failures occurred.")
endif()

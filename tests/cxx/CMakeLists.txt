include(CxxTests.cmake)

find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test)

include_directories(SYSTEM
  ${QtCore_INCLUDE_DIRS}
  ${Qt6Test_INCLUDE_DIRS}
  ${PARAVIEW_INCLUDE_DIRS}
  ${GTEST_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/tomviz)
include_directories(${PROJECT_SOURCE_DIR}/tomviz/pipeline)
include_directories(${PROJECT_SOURCE_DIR}/tomviz/animations)
include_directories(${PROJECT_SOURCE_DIR}/tomviz/loguru)
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
add_cxx_test(Variant)
add_cxx_test(Animation)
add_cxx_test(ScanID)
add_cxx_test(Utilities)
add_cxx_test(VolumeBricking)
add_cxx_test(SAM2SeedWidget)
add_cxx_test(ExternalEnvPersistence)
add_cxx_test(PythonEnvironmentCheck)
add_cxx_test(AutoExecute)
add_cxx_test(NodeDefinition)
add_cxx_test(TiltSeriesSinogram)
add_cxx_test(LabelSelection)
add_cxx_test(LabelMapSurface)
add_cxx_test(ExplodedGeometry)
add_cxx_test(LightingPresetStore)
# AcquisitionClient needs the `bottle` Python package (acquisition/requirements-dev.txt)
# to start its test server; not part of the conda build env, so disabled here.
# if(NOT WIN32)
#   add_cxx_qtest(AcquisitionClient PYTHONPATH "${CMAKE_SOURCE_DIR}/acquisition")
# endif()
add_cxx_qtest(LabelMap)
add_cxx_qtest(AnimationPlayback)
add_cxx_qtest(SegmentationColorMap)
add_cxx_qtest(ImageViewerMode)
add_cxx_qtest(MultiVolume)
add_cxx_qtest(LabelSurfaceRender)
add_cxx_qtest(PtychoWorkflow PYTHONPATH ${_pythonpath})
# initTestCase downloads the test data; QtTest aborts any test function
# after five minutes unless told otherwise, and a slow mirror can take
# longer than that.
set_tests_properties(PtychoWorkflow PROPERTIES TIMEOUT 1800
  ENVIRONMENT "QTEST_FUNCTION_TIMEOUT=1800000")
add_cxx_qtest(PyXRFWorkflow PYTHONPATH ${_pythonpath})
set_tests_properties(PyXRFWorkflow PROPERTIES TIMEOUT 1800
  ENVIRONMENT "QTEST_FUNCTION_TIMEOUT=1800000")

# Generate the executable
create_test_executable(tomvizTests)

target_link_libraries(tomvizTests Qt6::Test)

# NodeDefinitionTest round-trips every shipped operator descriptor
# through the definition-form mutators.
target_compile_definitions(tomvizTests PRIVATE
  "TOMVIZ_PYTHON_DIR=\"${PROJECT_SOURCE_DIR}/tomviz/python\"")

# Pipeline library tests
set(_pipeline_pythonpath
  "${PROJECT_SOURCE_DIR}/tomviz/python${_separator}${_pythonpath}")
add_executable(pipelineLibTests PipelineLibTest.cxx)
target_link_libraries(pipelineLibTests
  tomvizlib pybind11::embed GTest::gtest
  Qt6::Test ${EXTRA_LINK_LIB})
target_compile_definitions(pipelineLibTests PRIVATE
  "TOMVIZ_PYTHON_DIR=\"${PROJECT_SOURCE_DIR}/tomviz/python\"")
add_test(NAME "PipelineLib" COMMAND pipelineLibTests)
set_tests_properties("PipelineLib" PROPERTIES
  ENVIRONMENT "PYTHONPATH=${_pipeline_pythonpath}")

# Pipeline icon resources (must be compiled into each executable, not the
# static library, so Qt's resource system can initialise them).
set(PIPELINE_ICONS_QRC "${PROJECT_SOURCE_DIR}/tomviz/pipeline/pipeline_icons.qrc")

# Manual demo app for visually testing PipelineStripWidget
add_executable(pipelineStripDemo PipelineStripWidgetDemo.cxx ${PIPELINE_ICONS_QRC})
target_link_libraries(pipelineStripDemo tomvizlib)

# Manual demo app for visually testing pipeline Sinks with ParaView views
add_executable(pipelineSinkDemo PipelineSinkDemo.cxx ${PIPELINE_ICONS_QRC})
target_link_libraries(pipelineSinkDemo tomvizlib)

# Manual demo app with full pipeline: source -> transform -> 3 sinks + properties
add_executable(pipelineDemo PipelineDemo.cxx ${PIPELINE_ICONS_QRC})
target_compile_definitions(pipelineDemo PRIVATE
  "TOMVIZ_PYTHON_DIR=\"${PROJECT_SOURCE_DIR}/tomviz/python\"")
target_link_libraries(pipelineDemo tomvizlib)




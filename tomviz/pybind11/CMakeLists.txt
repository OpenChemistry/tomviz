find_package(TBB)

set(CMAKE_MODULE_LINKER_FLAGS "")
pybind11_add_module(_wrapping
  OperatorPythonWrapper.cxx
  PipelineStateManager.cxx
  Wrapping.cxx)
target_link_libraries(_wrapping
  PRIVATE tomvizcore VTK::CommonDataModel VTK::WrappingPythonCore)

set_target_properties(_wrapping PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${tomviz_python_binary_dir}/tomviz"
)
set_target_properties(_wrapping PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${tomviz_python_binary_dir}/tomviz"
)
set_target_properties(_wrapping PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${tomviz_python_binary_dir}/tomviz"
)

target_compile_options(_wrapping PRIVATE -D__TBB_NO_IMPLICIT_LINKAGE=1)
install(TARGETS _wrapping
    DESTINATION "${tomviz_python_install_dir}/tomviz"
    COMPONENT runtime)

pybind11_add_module(ctvlib
  ctvlib/ctvlib.cxx
  ctvlib/eigenConversion.h
  ctvlib/WrappingCtvlib.cxx)
target_link_libraries(ctvlib
  PRIVATE tomvizcore VTK::eigen ${TBB_LIBRARIES})

add_dependencies(ctvlib VTK::eigen)

include_directories(ctvlib ${TBB_INCLUDE_DIRS})

set_target_properties(ctvlib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${tomviz_python_binary_dir}/tomviz/_realtime"
)
set_target_properties(ctvlib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${tomviz_python_binary_dir}/_realtime"
)
set_target_properties(ctvlib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${tomviz_python_binary_dir}/_realtime"
)

install(TARGETS ctvlib
    DESTINATION "${tomviz_python_install_dir}/tomviz/_realtime"
    COMPONENT runtime)

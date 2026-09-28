find_package(TBB REQUIRED)

set(CMAKE_MODULE_LINKER_FLAGS "")

pybind11_add_module(ctvlib
  ctvlib/ctvlib.cxx
  ctvlib/eigenConversion.h
  ctvlib/WrappingCtvlib.cxx)
target_link_libraries(ctvlib
  PRIVATE tomvizcore VTK::eigen TBB::tbb)

add_dependencies(ctvlib VTK::eigen)

set_target_properties(ctvlib PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${tomviz_python_binary_dir}/tomviz/_realtime"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${tomviz_python_binary_dir}/tomviz/_realtime"
  LIBRARY_OUTPUT_DIRECTORY_DEBUG "${tomviz_python_binary_dir}/tomviz/_realtime"
)

install(TARGETS ctvlib
    DESTINATION "${tomviz_python_install_dir}/tomviz/_realtime"
    COMPONENT runtime)

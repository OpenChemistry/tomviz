set(CMAKE_MODULE_LINKER_FLAGS "")
pybind11_add_module(_wrapping OperatorPythonWrapper.cxx)

set_target_properties(_wrapping PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${tomviz_python_binary_dir}/tomviz"
)

target_compile_options(_wrapping PRIVATE -D__TBB_NO_IMPLICIT_LINKAGE=1)
install(TARGETS _wrapping
    DESTINATION "${tomviz_python_install_dir}/tomviz"
    COMPONENT runtime)

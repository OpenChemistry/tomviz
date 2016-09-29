set(CMAKE_MODULE_LINKER_FLAGS "")
pybind11_add_module(_wrapping OperatorPythonWrapper.cxx)

set_target_properties(_wrapping PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY "${tomviz_python_binary_dir}/tomviz"
)

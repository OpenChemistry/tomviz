add_library(tomvizcore SHARED PythonFactory.cxx)
target_link_libraries(tomvizcore PRIVATE ${PYTHON_LIBRARIES})
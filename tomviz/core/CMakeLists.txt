include(GenerateExportHeader)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(tomvizcore SHARED PythonFactory.cxx Variant.cxx)
target_compile_definitions(tomvizcore PRIVATE IS_TOMVIZ_CORE_BUILD)
generate_export_header(tomvizcore)
target_link_libraries(tomvizcore PRIVATE ${PYTHON_LIBRARIES})
install(TARGETS tomvizcore
  RUNTIME DESTINATION "${INSTALL_RUNTIME_DIR}"
  LIBRARY DESTINATION "${INSTALL_LIBRARY_DIR}"
  ARCHIVE DESTINATION "${INSTALL_ARCHIVE_DIR}")

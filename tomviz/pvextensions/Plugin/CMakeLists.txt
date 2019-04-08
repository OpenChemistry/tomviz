paraview_add_plugin(TomvizExtensions
  VERSION "1.0"
  SERVER_MANAGER_XML TomvizExtensions/TomvizExtensions.xml
  MODULES vtkOMETiffReader
  MODULE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/TomvizExtensions/vtk.module"
)

target_link_libraries(TomvizExtensions PUBLIC vtktiff vtkpugixml)

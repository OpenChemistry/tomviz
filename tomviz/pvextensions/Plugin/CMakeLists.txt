paraview_add_plugin(TomvizExtensions
  VERSION "1.0"
  MODULES vtkOMETiffReader
  MODULE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/TomvizExtensions/vtk.module"
)

target_link_libraries(TomvizExtensions PUBLIC VTK::tiff VTK::pugixml)

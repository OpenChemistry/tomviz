paraview_add_plugin(TomvizExtensions
  VERSION "1.0"
  FORCE_STATIC ON
)

target_link_libraries(TomvizExtensions PUBLIC VTK::tiff VTK::pugixml)

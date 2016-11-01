# Do not use automoc, autouic, and autorcc for ParaView extensions.
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTORCC OFF)

include(${PARAVIEW_USE_FILE})

set(outsrcs0)
set(outifaces0)
add_paraview_property_widget(outifaces0 outsrcs0
  TYPE "double_slider_widget"
  CLASS_NAME "pqDoubleSliderPropertyWidget")

qt5_wrap_cpp(moc_srcs
  pqDoubleSliderPropertyWidget.h
)

add_paraview_plugin(tomvizExtensions "1.0"
  SERVER_MANAGER_XML tomvizExtensions.xml
  GUI_INTERFACES ${outifaces0}
  SOURCES ${outsrcs0}
          ${moc_srcs}
          pqDoubleSliderPropertyWidget.cxx
)
target_link_libraries(tomvizExtensions PUBLIC pqApplicationComponents)

if (NOT APPLE)
  install(TARGETS tomvizExtensions
          RUNTIME DESTINATION "bin"
          LIBRARY DESTINATION "lib"
          ARCHIVE DESTINATION "lib"
          COMPONENT "runtime")
else()
  install(TARGETS tomvizExtensions
          RUNTIME DESTINATION "bin"
          LIBRARY DESTINATION "Applications/tomviz.app/Contents/Libraries"
          ARCHIVE DESTINATION "lib"
          COMPONENT "runtime")
  set_target_properties(tomvizExtensions PROPERTIES INSTALL_NAME_DIR "@executable_path/../Libraries")
endif()

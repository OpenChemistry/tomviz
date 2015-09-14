
include(${PARAVIEW_USE_FILE})

add_paraview_plugin(tomvizExtensions "1.0"
  SERVER_MANAGER_XML tomvizExtensions.xml
  SERVER_MANAGER_SOURCES
)

install(TARGETS tomvizExtensions
        RUNTIME DESTINATION "bin"
        LIBRARY DESTINATION "lib"
        ARCHIVE DESTINATION "lib"
	COMPONENT "runtime")

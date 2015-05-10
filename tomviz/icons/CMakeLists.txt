if (WIN32)
  set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTY INCLUDE_DIRECTORIES "")
  add_library(windows_icon_rc tomviz.rc dummy.cxx)
endif ()

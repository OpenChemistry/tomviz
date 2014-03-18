cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(temtomography)
find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})
if(NOT PARAVIEW_BUILD_QT_GUI)
  message(FATAL_ERROR
    "TEM tomography requires PARAVIEW_BUILD_QT_GUI to be enabled. "
    "Please rebuild ParaView with PARAVIEW_BUILD_QT_GUI set to TRUE")
endif()

set(SOURCES
  main.cxx
  MainWindow.cxx
  MainWindow.h
  )

qt4_wrap_cpp(MOC_SOURCES
  MainWindow.h)

qt4_wrap_ui(UI_SOURCES
  MainWindow.ui)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(temtomography ${SOURCES} ${UI_SOURCES})
set_target_properties(temtomography PROPERTIES AUTOMOC TRUE)
target_link_libraries(temtomography LINK_PRIVATE pqApplicationComponents)

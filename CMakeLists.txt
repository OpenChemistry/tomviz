cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(matviz)
find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})
if(NOT PARAVIEW_BUILD_QT_GUI)
  message(FATAL_ERROR
    "TEM tomography requires PARAVIEW_BUILD_QT_GUI to be enabled. "
    "Please rebuild ParaView with PARAVIEW_BUILD_QT_GUI set to TRUE")
endif()

set(SOURCES
  ActiveObjects.cxx
  ActiveObjects.h
  Behaviors.cxx
  Behaviors.h
  CentralWidget.cxx
  CentralWidget.h
  LoadDataReaction.cxx
  LoadDataReaction.h
  main.cxx
  MainWindow.cxx
  MainWindow.h
  PipelineWidget.cxx
  PipelineWidget.h
  Utilities.h
  )

qt4_wrap_cpp(MOC_SOURCES
  ActiveObjects.h
  Behaviors.h
  CentralWidget.h
  LoadDataReaction.h
  MainWindow.h
  PipelineWidget.h
  )

qt4_wrap_ui(UI_SOURCES
  CentralWidget.ui
  MainWindow.ui
  )

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(matviz ${SOURCES} ${UI_SOURCES})
set_target_properties(matviz PROPERTIES AUTOMOC TRUE)
target_link_libraries(matviz LINK_PRIVATE pqApplicationComponents)

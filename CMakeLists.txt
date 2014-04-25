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
  Module.cxx
  ModuleContour.cxx
  ModuleContour.h
  ModuleFactory.cxx
  ModuleFactory.h
  ModuleVolume.cxx
  Module.h
  ModuleManager.cxx
  ModuleManager.h
  ModuleMenu.cxx
  ModuleMenu.h
  ModuleOutline.cxx
  ModuleOutline.h
  PipelineWidget.cxx
  PipelineWidget.h
  Utilities.h
  )

qt4_wrap_ui(UI_SOURCES
  CentralWidget.ui
  MainWindow.ui
  )

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(matviz MACOSX_BUNDLE ${SOURCES} ${UI_SOURCES})
set_target_properties(matviz PROPERTIES AUTOMOC TRUE)
target_link_libraries(matviz LINK_PRIVATE pqApplicationComponents)
if(APPLE)
  install(TARGETS matviz DESTINATION Applications COMPONENT runtime)
else()
  install(TARGETS matviz DESTINATION bin COMPONENT runtime)
endif()

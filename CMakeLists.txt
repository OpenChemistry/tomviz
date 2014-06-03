cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(tomviz)
find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})
if(NOT PARAVIEW_BUILD_QT_GUI)
  message(FATAL_ERROR
    "TEM tomography requires PARAVIEW_BUILD_QT_GUI to be enabled. "
    "Please rebuild ParaView with PARAVIEW_BUILD_QT_GUI set to TRUE")
endif()

# We need to link to QtMain on Windows, this is much better in Qt 5 with recent
# CMake versions where a policy can invoke auto-linking of this library.
if(WIN32)
  find_package(Qt4 4.8 COMPONENTS QtGui QtMain REQUIRED)
endif()

option(ENABLE_DAX_ACCELERATION "Enable Accelerated Algorithms" OFF)
if(ENABLE_DAX_ACCELERATION)
  find_package(Dax REQUIRED)
  DaxConfigureTBB(REQUIRED)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dax)

  set(accel_srcs
    dax/ModuleStreamingContour.cxx
    )

endif()

set(SOURCES
  ActiveObjects.cxx
  ActiveObjects.h
  Behaviors.cxx
  Behaviors.h
  CentralWidget.cxx
  CentralWidget.h
  DataSource.cxx
  DataSource.h
  LoadDataReaction.cxx
  LoadDataReaction.h
  main.cxx
  MainWindow.cxx
  MainWindow.h
  ModuleContour.cxx
  ModuleContour.h
  ModuleThreshold.cxx
  ModuleThreshold.h
  Module.cxx
  ModuleFactory.cxx
  ModuleFactory.h
  Module.h
  ModuleManager.cxx
  ModuleManager.h
  ModuleMenu.cxx
  ModuleMenu.h
  ModuleOrthogonalSlice.cxx
  ModuleOrthogonalSlice.h
  ModuleOutline.cxx
  ModuleOutline.h
  ModulePropertiesPanel.cxx
  ModulePropertiesPanel.h
  ModuleVolume.cxx
  PipelineWidget.cxx
  PipelineWidget.h
  ProgressBehavior.cxx
  ProgressBehavior.h
  RecentFilesMenu.cxx
  RecentFilesMenu.h
  Utilities.h
  )

qt4_wrap_ui(UI_SOURCES
  CentralWidget.ui
  MainWindow.ui
  ModulePropertiesPanel.ui
  )

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(tomviz WIN32 MACOSX_BUNDLE ${SOURCES} ${UI_SOURCES} ${accel_srcs})
if(APPLE)
  set_target_properties(tomviz PROPERTIES OUTPUT_NAME "TomViz")
endif()

set_target_properties(tomviz PROPERTIES AUTOMOC TRUE)

target_link_libraries(tomviz
  LINK_PRIVATE
    pqApplicationComponents
    vtkPVServerManagerRendering
  )
if(WIN32)
  target_link_libraries(tomviz LINK_PRIVATE ${QT_QTMAIN_LIBRARY})
endif()
if(APPLE)
  install(TARGETS tomviz DESTINATION Applications COMPONENT runtime)
else()
  install(TARGETS tomviz DESTINATION bin COMPONENT runtime)
endif()

if(ENABLE_DAX_ACCELERATION)
  target_link_libraries(tomviz LINK_PRIVATE ${TBB_LIBRARIES})

  #set the dax backend to tbb explicitly as the histogram is
  #computed using dax.
  set_target_properties(tomviz PROPERTIES COMPILE_DEFINITIONS
                        "DAX_DEVICE_ADAPTER=DAX_DEVICE_ADAPTER_TBB")
endif()

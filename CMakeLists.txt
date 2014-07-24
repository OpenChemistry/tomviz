cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(tomviz)
find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})
if(NOT PARAVIEW_BUILD_QT_GUI)
  message(FATAL_ERROR
    "TomViz requires PARAVIEW_BUILD_QT_GUI to be enabled. "
    "Please rebuild ParaView with PARAVIEW_BUILD_QT_GUI set to TRUE.")
endif()
if(NOT PARAVIEW_ENABLE_PYTHON)
  message(FATAL_ERROR
    "TomViz requires PARAVIEW_ENABLE_PYTHON to be enabled. "
    "Please rebuild ParaView with PARAVIEW_ENABLE_PYTHON set to TRUE.")
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

  add_subdirectory(dax)

  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/dax)
  set(accel_srcs
    dax/ModuleStreamingContour.cxx
    dax/ModuleAccelThreshold.cxx
    )
endif()

set(SOURCES
  ActiveObjects.cxx
  ActiveObjects.h
  AddExpressionReaction.cxx
  AddExpressionReaction.h
  Behaviors.cxx
  Behaviors.h
  CentralWidget.cxx
  CentralWidget.h
  CloneDataReaction.cxx
  CloneDataReaction.h
  DataSource.cxx
  DataSource.h
  DeleteDataReaction.cxx
  DeleteDataReaction.h
  EditPythonOperatorDialog.cxx
  EditPythonOperatorDialog.h
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
  Operator.cxx
  Operator.h
  OperatorPython.cxx
  OperatorPython.h
  OperatorsWidget.cxx
  OperatorsWidget.h
  PipelineWidget.cxx
  PipelineWidget.h
  ProgressBehavior.cxx
  ProgressBehavior.h
  RecentFilesMenu.cxx
  RecentFilesMenu.h
  ResetReaction.cxx
  ResetReaction.h
  SaveLoadStateReaction.cxx
  SaveLoadStateReaction.h
  Utilities.cxx
  Utilities.h
  ViewPropertiesPanel.cxx
  ViewPropertiesPanel.h
  )

qt4_wrap_ui(UI_SOURCES
  CentralWidget.ui
  EditPythonOperatorDialog.ui
  MainWindow.ui
  ModulePropertiesPanel.ui
  ViewPropertiesPanel.ui
  )
qt4_add_resources(RCC_SOURCES resources.qrc)

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(TomVizPythonConfig.h.in TomVizPythonConfig.h @ONLY)

add_executable(tomviz WIN32 MACOSX_BUNDLE ${SOURCES} ${UI_SOURCES}
  ${RCC_SOURCES} ${accel_srcs})
if(APPLE)
  set_target_properties(tomviz PROPERTIES OUTPUT_NAME "TomViz")
endif()

set_target_properties(tomviz PROPERTIES AUTOMOC TRUE)

target_link_libraries(tomviz
  LINK_PRIVATE
    pqApplicationComponents
    vtkPVServerManagerRendering
    vtkpugixml
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
  target_link_libraries(tomviz
                        LINK_PRIVATE
                        tomvizStreaming
                        tomvizThreshold
                        ${TBB_LIBRARIES})

  #set the dax backend to tbb explicitly as the histogram is
  #computed using dax.
  set_target_properties(tomviz PROPERTIES COMPILE_DEFINITIONS
                        "DAX_DEVICE_ADAPTER=DAX_DEVICE_ADAPTER_TBB")
endif()

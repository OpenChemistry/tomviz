cmake_minimum_required(VERSION 2.8.8 FATAL_ERROR)
project(tomviz)

set (tomviz_version_major 0)
set (tomviz_version_minor 3)
set (tomviz_version_patch 0)
set (tomviz_version_suffix)
set (tomviz_version "${tomviz_version_major}.${tomviz_version_minor}.${tomviz_version_patch}")

# Location where python modules will be installed.
set(tomviz_python_install_dir "lib/tomviz-${tomviz_version}/site-packages")
if(APPLE)
  set(tomviz_python_install_dir "Applications/tomviz.app/Contents/Python")
endif()

find_package(ParaView REQUIRED)
include(${PARAVIEW_USE_FILE})
if(NOT PARAVIEW_BUILD_QT_GUI)
  message(FATAL_ERROR
    "Tomviz requires PARAVIEW_BUILD_QT_GUI to be enabled. "
    "Please rebuild ParaView with PARAVIEW_BUILD_QT_GUI set to TRUE.")
endif()
if(NOT PARAVIEW_ENABLE_PYTHON)
  message(FATAL_ERROR
    "Tomviz requires PARAVIEW_ENABLE_PYTHON to be enabled. "
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
  AddAlignReaction.cxx
  AddAlignReaction.h
  AddExpressionReaction.cxx
  AddExpressionReaction.h
  AddPythonTransformReaction.cxx
  AlignWidget.cxx
  AlignWidget.h
  Behaviors.cxx
  Behaviors.h
  CentralWidget.cxx
  CentralWidget.h
  CloneDataReaction.cxx
  CloneDataReaction.h
  DataPropertiesPanel.cxx
  DataPropertiesPanel.h
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
  Module.cxx
  Module.h
  ModuleContour.cxx
  ModuleContour.h
  ModuleFactory.cxx
  ModuleFactory.h
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
  ModuleSlice.cxx
  ModuleSlice.h
  ModuleThreshold.cxx
  ModuleThreshold.h
  ModuleVolume.cxx
  ModuleVolume.h
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
  SaveDataReaction.cxx
  SaveLoadStateReaction.cxx
  SaveLoadStateReaction.h
  Utilities.cxx
  Utilities.h
  ViewPropertiesPanel.cxx
  ViewPropertiesPanel.h
  vtkNonOrthoImagePlaneWidget.cxx
  vtkNonOrthoImagePlaneWidget.h
  )

set(python_files
  MisalignImgs_Uniform.py
  Align_Images.py
  Recon_DFT.py
  Crop_Data.py
  FFT_AbsLog.py
  Shift_Stack_Uniformly.py
  Square_Root_Data.py
  Subtract_TiltSer_Background.py
  MisalignImgs_Gaussian.py
  )
unset(python_h_files)
foreach(file ${python_files})
  set(file Python/${file})
  get_filename_component(file_we ${file} NAME_WE)
  set(src  ${CMAKE_CURRENT_SOURCE_DIR}/${file})
  set(res  ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.cxx)
  set(resh ${CMAKE_CURRENT_BINARY_DIR}/${file_we}.h)
  list(APPEND python_h_files ${resh})
  add_custom_command(
    OUTPUT ${res} ${resh}
    DEPENDS ${src} vtkEncodeString
    COMMAND vtkEncodeString
    ARGS ${res} ${src} ${file_we} --build-header VTK_EXPORT vtkObject.h
    )
  list(APPEND SOURCES ${res})
endforeach()

qt4_wrap_ui(UI_SOURCES
  AboutDialog.ui
  CentralWidget.ui
  DataPropertiesPanel.ui
  EditPythonOperatorDialog.ui
  MainWindow.ui
  ModulePropertiesPanel.ui
  ViewPropertiesPanel.ui
  )
qt4_add_resources(RCC_SOURCES resources.qrc)

if(APPLE)
  list(APPEND SOURCES icons/tomviz.icns)
  set(MACOSX_BUNDLE_ICON_FILE tomviz.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${tomviz_version}")
  set_source_files_properties(icons/tomviz.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
  list(APPEND SOURCES icons/tomviz.rc)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(TomVizPythonConfig.h.in TomVizPythonConfig.h @ONLY)

add_executable(tomviz WIN32 MACOSX_BUNDLE ${SOURCES} ${UI_SOURCES}
  ${RCC_SOURCES} ${accel_srcs})

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

# Install the tomviz Python files.
install(DIRECTORY Python/tomviz
       DESTINATION "${tomviz_python_install_dir}"
       USE_SOURCE_PERMISSIONS
       COMPONENT runtime)

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

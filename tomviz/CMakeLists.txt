# These dependencies are inherited from ParaView, we will ideally trim away
# Help and UiTools in the future (at least UiTools).
find_package(Qt5 REQUIRED COMPONENTS Help Network Widgets UiTools)
include_directories(SYSTEM
  ${Qt5Help_INCLUDE_DIRS}
  ${Qt5Network_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${Qt5UiTools_INCLUDE_DIRS})

find_package(ParaView REQUIRED)

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
if(NOT PARAVIEW_QT_VERSION STREQUAL "5")
  message(FATAL_ERROR
    "Tomviz requires PARAVIEW_QT_VERSION to be 5, please rebuild ParaView.")
endif()
include_directories(SYSTEM ${PARAVIEW_INCLUDE_DIRS})

find_package(ITK 4.9)
if(ITK_FOUND AND NOT ITK_WRAP_PYTHON)
  message(FATAL_ERROR
    "Tomviz requires ITK_WRAP_PYTHON to be enabled. "
    "Please rebuild ITK with ITK_WRAP_PYTHON set to TRUE.")
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

add_subdirectory(pvextensions)

set(SOURCES
  ActiveObjects.cxx
  ActiveObjects.h
  AddAlignReaction.cxx
  AddAlignReaction.h
  AddExpressionReaction.cxx
  AddExpressionReaction.h
  AddPythonTransformReaction.cxx
  AddRenderViewContextMenuBehavior.cxx
  AddRenderViewContextMenuBehavior.h
  AddResampleReaction.cxx
  AddResampleReaction.h
  AddRotateAlignReaction.cxx
  AddRotateAlignReaction.h
  AlignWidget.cxx
  AlignWidget.h
  Behaviors.cxx
  Behaviors.h
  CentralWidget.cxx
  CentralWidget.h
  CloneDataReaction.cxx
  CloneDataReaction.h
  ConvertToFloatOperator.cxx
  ConvertToFloatOperator.h
  ConvertToFloatReaction.cxx
  ConvertToFloatReaction.h
  CropReaction.cxx
  CropReaction.h
  CropOperator.cxx
  CropOperator.h
  SelectVolumeWidget.cxx
  SelectVolumeWidget.h
  DataPropertiesPanel.cxx
  DataPropertiesPanel.h
  DataSource.cxx
  DataSource.h
  DataTransformMenu.cxx
  DataTransformMenu.h
  DeleteDataReaction.cxx
  DeleteDataReaction.h
  DoubleSliderWidget.cxx
  DoubleSliderWidget.h
  DoubleSpinBox.cxx
  DoubleSpinBox.h
  EditOperatorDialog.cxx
  EditOperatorDialog.h
  EditOperatorWidget.cxx
  EditOperatorWidget.h
  HistogramWidget.h
  HistogramWidget.cxx
  IntSliderWidget.cxx
  IntSliderWidget.h
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
  ModuleSegment.cxx
  ModuleSegment.h
  ModuleSlice.cxx
  ModuleSlice.h
  ModuleThreshold.cxx
  ModuleThreshold.h
  ModuleVolume.cxx
  ModuleVolume.h
  MoveActiveObject.cxx
  MoveActiveObject.h
  Operator.cxx
  Operator.h
  OperatorFactory.cxx
  OperatorFactory.h
  OperatorPython.cxx
  OperatorPython.h
  OperatorResult.cxx
  OperatorResult.h
  PipelineModel.cxx
  PipelineModel.h
  PipelineView.cxx
  PipelineView.h
  PipelineWorker.cxx
  PipelineWorker.h
  ProgressBehavior.cxx
  ProgressBehavior.h
  ProgressDialogManager.cxx
  ProgressDialogManager.h
  PythonGeneratedDatasetReaction.cxx
  PythonGeneratedDatasetReaction.h
  RecentFilesMenu.cxx
  RecentFilesMenu.h
  ReconstructionOperator.cxx
  ReconstructionOperator.h
  ReconstructionReaction.cxx
  ReconstructionReaction.h
  ReconstructionWidget.h
  ReconstructionWidget.cxx
  ResetReaction.cxx
  ResetReaction.h
  RotateAlignWidget.cxx
  RotateAlignWidget.h
  SaveDataReaction.cxx
  SaveLoadStateReaction.cxx
  SaveLoadStateReaction.h
  SaveScreenshotReaction.cxx
  SaveScreenshotReaction.h
  ScaleActorBehavior.cxx
  ScaleActorBehavior.h
  SetScaleReaction.cxx
  SetScaleReaction.h
  SetTiltAnglesOperator.cxx
  SetTiltAnglesOperator.h
  SetTiltAnglesReaction.cxx
  SetTiltAnglesReaction.h
  SpinBox.cxx
  SpinBox.h
  ToggleDataTypeReaction.h
  ToggleDataTypeReaction.cxx
  TomographyReconstruction.h
  TomographyReconstruction.cxx
  TomographyTiltSeries.h
  TomographyTiltSeries.cxx
  TranslateAlignOperator.h
  TranslateAlignOperator.cxx
  Utilities.cxx
  Utilities.h
  ViewFrameActions.cxx
  ViewFrameActions.h
  ViewPropertiesPanel.cxx
  ViewPropertiesPanel.h
  ViewMenuManager.cxx
  ViewMenuManager.h
  vtkChartHistogram.cxx
  vtkChartHistogram.h
  vtkChartHistogramColorOpacityEditor.cxx
  vtkChartHistogramColorOpacityEditor.h
  vtkCustomPiecewiseControlPointsItem.cxx
  vtkCustomPiecewiseControlPointsItem.h
  vtkNonOrthoImagePlaneWidget.cxx
  vtkNonOrthoImagePlaneWidget.h
  WelcomeDialog.cxx
  WelcomeDialog.h
  )

set(python_files
  BinaryThreshold.py
  ConnectedComponents.py
  MisalignImgs_Uniform.py
  Align_Images.py
  Recon_DFT.py
  Recon_DFT_constraint.py
  Recon_WBP.py
  Recon_ART.py
  Crop_Data.py
  FFT_AbsLog.py
  Shift_Stack_Uniformly.py
  Shift3D.py
  Square_Root_Data.py
  Subtract_TiltSer_Background.py
  Subtract_TiltSer_Background_Auto.py
  MisalignImgs_Gaussian.py
  NormalizeTiltSeries.py
  Rotate3D.py
  HannWindow3D.py
  GradientMagnitude_Sobel.py
  GradientMagnitude2D_Sobel.py
  LaplaceFilter.py
  GaussianFilter.py
  MedianFilter.py
  Resample.py
  deleteSlices.py
  ClearVolume.py
  ConstantDataset.py
  RandomParticles.py
  GenerateTiltSeries.py
  SetNegativeVoxelsToZero.py
  STEM_probe.py
  InvertData.py
  Pad_Data.py
  BinVolumeByTwo.py
  BinTiltSeriesByTwo.py
  )

set(json_files
  BinaryThreshold.json
  ConnectedComponents.json
  )

set(resource_files
  matplotlib_cmaps.json
  )

# Make symlinks where possible, file copies where necessary in the build tree.
if(UNIX)
  set(script_cmd "create_symlink")
else()
  set(script_cmd "copy_if_different")
endif()
make_directory("${tomviz_BINARY_DIR}/share/tomviz/scripts")
foreach(file ${python_files})
  list(APPEND SOURCES "python/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/scripts/${file}")
  install(FILES "python/${file}"
    DESTINATION "${tomviz_data_install_dir}/scripts"
    COMPONENT "Scripts")
endforeach()
foreach(file ${json_files})
  list(APPEND SOURCES "python/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/scripts/${file}")
  install(FILES "python/${file}"
    DESTINATION "${tomviz_data_install_dir}/scripts"
    COMPONENT "Scripts")
endforeach()
foreach(file ${resource_files})
  list(APPEND SOURCES "resources/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/resources/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/${file}")
  install(FILES "resources/${file}"
    DESTINATION "${tomviz_data_install_dir}"
    COMPONENT runtime)
endforeach()

qt5_wrap_ui(UI_SOURCES
  AboutDialog.ui
  CentralWidget.ui
  SelectVolumeWidget.ui
  DataPropertiesPanel.ui
  EditPythonOperatorWidget.ui
  MainWindow.ui
  ModulePropertiesPanel.ui
  RotateAlignWidget.ui
  ReconstructionWidget.ui
  ViewPropertiesPanel.ui
  WelcomeDialog.ui
  )
qt5_add_resources(RCC_SOURCES resources.qrc)

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

configure_file(tomvizConfig.h.in tomvizConfig.h @ONLY)
configure_file(tomvizPythonConfig.h.in tomvizPythonConfig.h @ONLY)

add_executable(tomviz WIN32 MACOSX_BUNDLE ${SOURCES} ${UI_SOURCES}
  ${RCC_SOURCES} ${accel_srcs})

set_target_properties(tomviz PROPERTIES AUTOMOC TRUE)
target_link_libraries(tomviz
  pqApplicationComponents
  vtkPVServerManagerRendering
  vtkjsoncpp
  vtkpugixml
  tomvizExtensions)
if(WIN32)
  target_link_libraries(tomviz Qt5::WinMain)
endif()
if(APPLE)
  install(TARGETS tomviz DESTINATION Applications COMPONENT runtime)
else()
  install(TARGETS tomviz DESTINATION bin COMPONENT runtime)
endif()

# Install the tomviz Python files.
install(DIRECTORY python/tomviz
       DESTINATION "${tomviz_python_install_dir}"
       USE_SOURCE_PERMISSIONS
       COMPONENT runtime)
if(tomviz_data_DIR)
  add_definitions(-DTOMVIZ_DATA)
  install(DIRECTORY "${tomviz_data_DIR}"
    DESTINATION "${tomviz_data_install_dir}"
    USE_SOURCE_PERMISSIONS
    COMPONENT runtime)
endif()

if(ENABLE_DAX_ACCELERATION)
  target_link_libraries(tomviz
      tomvizStreaming
      tomvizThreshold
      ${TBB_LIBRARIES})

  #set the dax backend to tbb explicitly as the histogram is
  #computed using dax.
  set_target_properties(tomviz PROPERTIES COMPILE_DEFINITIONS
    "DAX_DEVICE_ADAPTER=DAX_DEVICE_ADAPTER_TBB")
endif()

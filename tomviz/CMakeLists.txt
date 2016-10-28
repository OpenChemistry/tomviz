include_directories(SYSTEM
  ${Qt5Network_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${Qt5UiTools_INCLUDE_DIRS})

include_directories(SYSTEM ${PARAVIEW_INCLUDE_DIRS})

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

include_directories(SYSTEM ${PROJECT_SOURCE_DIR}/thirdparty/pybind11/include)

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
  InterfaceBuilder.h
  InterfaceBuilder.cxx
  IntSliderWidget.cxx
  IntSliderWidget.h
  LoadDataReaction.cxx
  LoadDataReaction.h
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
  OperatorDialog.cxx
  OperatorDialog.h
  OperatorFactory.cxx
  OperatorFactory.h
  OperatorPropertiesPanel.cxx
  OperatorPropertiesPanel.h
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
  )

set(exec_sources
  main.cxx
  MainWindow.cxx
  MainWindow.h
  WelcomeDialog.cxx
  WelcomeDialog.h
)

set(python_files
  BinaryThreshold.py
  ConnectedComponents.py
  OtsuMultipleThreshold.py
  BinaryDilate.py
  BinaryErode.py
  BinaryOpen.py
  BinaryClose.py
  MisalignImgs_Uniform.py
  AutoTiltAxisAlignment.py
  AutoTiltImageAlignment.py
  Recon_DFT.py
  Recon_DFT_constraint.py
  Recon_WBP.py
  Recon_ART.py
  Recon_SIRT.py
  Recon_TV_minimization.py
  Crop_Data.py
  FFT_AbsLog.py
  Shift_Stack_Uniformly.py
  Shift3D.py
  Square_Root_Data.py
  Subtract_TiltSer_Background.py
  Subtract_TiltSer_Background_Auto.py
  MedianFilterTiltSeries.py
  MisalignImgs_Gaussian.py
  NormalizeTiltSeries.py
  Rotate3D.py
  HannWindow3D.py
  GradientMagnitude_Sobel.py
  GradientMagnitude2D_Sobel.py
  LaplaceFilter.py
  GaussianFilter.py
  GaussianFilterTiltSeries.py
  PeronaMalikAnisotropicDiffusion.py
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
  OtsuMultipleThreshold.json
  BinaryDilate.json
  BinaryErode.json
  BinaryOpen.json
  BinaryClose.json
  MedianFilterTiltSeries.json
  Shift_Stack_Uniformly.json
  Pad_Data.json
  Resample.json
  GaussianFilter.json
  GaussianFilterTiltSeries.json
  PeronaMalikAnisotropicDiffusion.json
  MedianFilter.json
  Rotate3D.json
  GenerateTiltSeries.json
  Recon_ART.json
  Recon_TV_minimization.json
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
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/scripts")
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

set(tomviz_python_modules
  __init__.py
  _internal.py
  operators.py
  utils.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz")
foreach(file ${tomviz_python_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/${file}"
    "${tomviz_python_binary_dir}/tomviz/${file}")
endforeach()

# Install the tomviz Python files.
install(DIRECTORY "${tomviz_SOURCE_DIR}/tomviz/python/tomviz"
       DESTINATION "${tomviz_python_install_dir}"
       USE_SOURCE_PERMISSIONS
       COMPONENT runtime)

qt5_wrap_ui(UI_SOURCES
  AboutDialog.ui
  CentralWidget.ui
  SelectVolumeWidget.ui
  DataPropertiesPanel.ui
  EditPythonOperatorWidget.ui
  ModulePropertiesPanel.ui
  RotateAlignWidget.ui
  ReconstructionWidget.ui
  ViewPropertiesPanel.ui
  MainWindow.ui
  WelcomeDialog.ui
)
qt5_add_resources(RCC_SOURCES resources.qrc)

if(APPLE)
  list(APPEND exec_sources icons/tomviz.icns)
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

add_library(tomvizlib STATIC ${SOURCES} ${UI_SOURCES} ${accel_srcs})
set_target_properties(tomvizlib
  PROPERTIES OUTPUT_NAME tomviz)

set_property(TARGET tomvizlib PROPERTY POSITION_INDEPENDENT_CODE TRUE)

add_executable(tomviz WIN32 MACOSX_BUNDLE ${exec_sources} ${RCC_SOURCES})
target_link_libraries(tomviz tomvizlib)

set_target_properties(tomvizlib PROPERTIES AUTOMOC TRUE)
set_target_properties(tomviz PROPERTIES AUTOMOC TRUE)

target_link_libraries(tomvizlib
  pqApplicationComponents
  vtkPVServerManagerRendering
  vtkjsoncpp
  vtkpugixml
  tomvizExtensions)
if(WIN32)
  target_link_libraries(tomvizlib Qt5::WinMain)
endif()
if(APPLE)
  install(TARGETS tomviz DESTINATION Applications COMPONENT runtime)
else()
  install(TARGETS tomviz DESTINATION bin COMPONENT runtime)
endif()

if(tomviz_data_DIR)
  add_definitions(-DTOMVIZ_DATA)
  install(DIRECTORY "${tomviz_data_DIR}"
    DESTINATION "${tomviz_data_install_dir}"
    USE_SOURCE_PERMISSIONS
    COMPONENT runtime)
endif()

if(ENABLE_DAX_ACCELERATION)
  target_link_libraries(tomvizlib
      tomvizStreaming
      tomvizThreshold
      ${TBB_LIBRARIES})

  #set the dax backend to tbb explicitly as the histogram is
  #computed using dax.
  set_target_properties(tomvizlib PROPERTIES COMPILE_DEFINITIONS
    "DAX_DEVICE_ADAPTER=DAX_DEVICE_ADAPTER_TBB")
endif()

add_subdirectory(pybind11)

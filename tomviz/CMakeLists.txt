find_package(OpenGL REQUIRED)

include_directories(SYSTEM
  ${Qt5Network_INCLUDE_DIRS}
  ${Qt5Widgets_INCLUDE_DIRS}
  ${Qt5Concurrent_INCLUDE_DIRS}
  ${PARAVIEW_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/thirdparty/pybind11/include)

add_subdirectory(pvextensions)

set(SOURCES
  AbstractDataModel.h
  AbstractDataModel.cxx
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
  AlignWidget.cxx
  AlignWidget.h
  AxesReaction.cxx
  AxesReaction.h
  Behaviors.cxx
  Behaviors.h
  CameraReaction.cxx
  CameraReaction.h
  CentralWidget.cxx
  CentralWidget.h
  CloneDataReaction.cxx
  CloneDataReaction.h
  ConvertToFloatReaction.cxx
  ConvertToFloatReaction.h
  CropReaction.cxx
  CropReaction.h
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
  DockerUtilities.cxx
  DockerUtilities.h
  DoubleSliderWidget.cxx
  DoubleSliderWidget.h
  DoubleSpinBox.cxx
  DoubleSpinBox.h
  DuplicateModuleReaction.h
  DuplicateModuleReaction.cxx
  EmdFormat.cxx
  EmdFormat.h
  ExportDataReaction.cxx
  ExportDataReaction.h
  FileFormatManager.cxx
  FileFormatManager.h
  GradientOpacityWidget.h
  GradientOpacityWidget.cxx
  HistogramManager.h
  HistogramManager.cxx
  HistogramWidget.h
  HistogramWidget.cxx
  Histogram2DWidget.h
  Histogram2DWidget.cxx
  ImageStackDialog.h
  ImageStackDialog.cxx
  ImageStackModel.h
  ImageStackModel.cxx
  InterfaceBuilder.h
  InterfaceBuilder.cxx
  IntSliderWidget.cxx
  IntSliderWidget.h
  LoadDataReaction.cxx
  LoadDataReaction.h
  LoadPaletteReaction.cxx
  LoadPaletteReaction.h
  LoadStackReaction.cxx
  LoadStackReaction.h
  Logger.cxx
  Logger.h
  MergeImagesDialog.cxx
  MergeImagesDialog.h
  MergeImagesReaction.cxx
  MergeImagesReaction.h
  MoleculeSource.cxx
  MoleculeSource.h
  MoleculeProperties.cxx
  MoleculeProperties.h
  MoleculePropertiesPanel.cxx
  MoleculePropertiesPanel.h
  MoveActiveObject.cxx
  MoveActiveObject.h
  Pipeline.cxx
  Pipeline.h
  PipelineExecutor.cxx
  PipelineExecutor.h
  PipelineManager.cxx
  PipelineManager.h
  PipelineModel.cxx
  PipelineModel.h
  PipelineView.cxx
  PipelineView.h
  PipelineWorker.cxx
  PipelineWorker.h
  PipelineSettingsDialog.cxx
  PipelineSettingsDialog.h
  ProgressDialog.cxx
  ProgressDialog.h
  ProgressDialogManager.cxx
  ProgressDialogManager.h
  PythonGeneratedDatasetReaction.cxx
  PythonGeneratedDatasetReaction.h
  PythonReader.cxx
  PythonReader.h
  PythonUtilities.cxx
  PythonUtilities.h
  PythonWriter.cxx
  PythonWriter.h
  QVTKGLWidget.cxx
  QVTKGLWidget.h
  RAWFileReaderDialog.h
  RAWFileReaderDialog.cxx
  RecentFilesMenu.cxx
  RecentFilesMenu.h
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
  SaveWebReaction.cxx
  ScaleActorBehavior.cxx
  ScaleActorBehavior.h
  SetDataTypeReaction.h
  SetDataTypeReaction.cxx
  SetTiltAnglesReaction.cxx
  SetTiltAnglesReaction.h
  SpinBox.cxx
  SpinBox.h
  TomographyReconstruction.h
  TomographyReconstruction.cxx
  TomographyTiltSeries.h
  TomographyTiltSeries.cxx
  Utilities.cxx
  Utilities.h
  Variant.cxx
  Variant.h
  ViewFrameActions.cxx
  ViewFrameActions.h
  ViewMenuManager.cxx
  ViewMenuManager.h
  vtkChartGradientOpacityEditor.cxx
  vtkChartGradientOpacityEditor.h
  vtkChartHistogram.cxx
  vtkChartHistogram.h
  vtkChartHistogramColorOpacityEditor.cxx
  vtkChartHistogramColorOpacityEditor.h
  vtkChartTransfer2DEditor.cxx
  vtkChartTransfer2DEditor.h
  vtkTransferFunctionBoxItem.h
  vtkTransferFunctionBoxItem.cxx
  vtkCustomPiecewiseControlPointsItem.cxx
  vtkCustomPiecewiseControlPointsItem.h
  vtkLengthScaleRepresentation.h
  vtkLengthScaleRepresentation.cxx
  vtkNonOrthoImagePlaneWidget.cxx
  vtkNonOrthoImagePlaneWidget.h
  vtkVolumeScaleRepresentation.h
  vtkVolumeScaleRepresentation.cxx
  WebExportWidget.cxx
  WebExportWidget.h
)

list(APPEND SOURCES
  modules/Module.cxx
  modules/Module.h
  modules/ModuleContour.cxx
  modules/ModuleContour.h
  modules/ModuleContourWidget.cxx
  modules/ModuleContourWidget.h
  modules/ModuleFactory.cxx
  modules/ModuleFactory.h
  modules/ModuleManager.cxx
  modules/ModuleManager.h
  modules/ModuleScaleCube.cxx
  modules/ModuleScaleCube.h
  modules/ModuleScaleCubeWidget.cxx
  modules/ModuleScaleCubeWidget.h
  modules/ModuleMenu.cxx
  modules/ModuleMenu.h
  modules/ModuleMolecule.cxx
  modules/ModuleMolecule.h
  modules/ModuleOrthogonalSlice.cxx
  modules/ModuleOrthogonalSlice.h
  modules/ModuleOutline.cxx
  modules/ModuleOutline.h
  modules/ModulePropertiesPanel.cxx
  modules/ModulePropertiesPanel.h
  modules/ModuleRuler.cxx
  modules/ModuleRuler.h
  modules/ModuleSegment.cxx
  modules/ModuleSegment.h
  modules/ModuleSlice.cxx
  modules/ModuleSlice.h
  modules/ModuleThreshold.cxx
  modules/ModuleThreshold.h
  modules/ModuleVolume.cxx
  modules/ModuleVolume.h
  modules/ModuleVolumeWidget.cxx
  modules/ModuleVolumeWidget.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/modules)

# acquisition/
list(APPEND SOURCES
  acquisition/AcquisitionWidget.cxx
  acquisition/AcquisitionWidget.h
  acquisition/AcquisitionClient.cxx
  acquisition/AcquisitionClient.h
  acquisition/AdvancedFormatWidget.cxx
  acquisition/AdvancedFormatWidget.h
  acquisition/BasicFormatWidget.cxx
  acquisition/BasicFormatWidget.h
  acquisition/Connection.cxx
  acquisition/Connection.h
  acquisition/ConnectionsWidget.cxx
  acquisition/ConnectionsWidget.h
  acquisition/ConnectionDialog.cxx
  acquisition/ConnectionDialog.h
  acquisition/CustomFormatWidget.cxx
  acquisition/CustomFormatWidget.h
  acquisition/JsonRpcClient.cxx
  acquisition/JsonRpcClient.h
  acquisition/PassiveAcquisitionWidget.cxx
  acquisition/PassiveAcquisitionWidget.h
  acquisition/RegexGroupDialog.cxx
  acquisition/RegexGroupDialog.h
  acquisition/RegexGroupsWidget.cxx
  acquisition/RegexGroupsWidget.h
  acquisition/RegexGroupSubstitution.cxx
  acquisition/RegexGroupSubstitution.h
  acquisition/RegexGroupSubstitutionDialog.cxx
  acquisition/RegexGroupSubstitutionDialog.h
  acquisition/RegexGroupsSubstitutionsWidget.cxx
  acquisition/RegexGroupsSubstitutionsWidget.h
  acquisition/StartServerDialog.cxx
  acquisition/StartServerDialog.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/acquisition)

list(APPEND SOURCES
  operators/ConvertToFloatOperator.cxx
  operators/ConvertToFloatOperator.h
  operators/CustomPythonOperatorWidget.cxx
  operators/CustomPythonOperatorWidget.h
  operators/CropOperator.cxx
  operators/CropOperator.h
  operators/EditOperatorDialog.cxx
  operators/EditOperatorDialog.h
  operators/EditOperatorWidget.cxx
  operators/EditOperatorWidget.h
  operators/Operator.cxx
  operators/Operator.h
  operators/OperatorDialog.cxx
  operators/OperatorDialog.h
  operators/OperatorFactory.cxx
  operators/OperatorFactory.h
  operators/OperatorPropertiesPanel.cxx
  operators/OperatorPropertiesPanel.h
  operators/OperatorPython.cxx
  operators/OperatorPython.h
  operators/OperatorResult.cxx
  operators/OperatorResult.h
  operators/OperatorResultPropertiesPanel.cxx
  operators/OperatorResultPropertiesPanel.h
  operators/OperatorWidget.cxx
  operators/OperatorWidget.h
  operators/ReconstructionOperator.cxx
  operators/ReconstructionOperator.h
  operators/SetTiltAnglesOperator.cxx
  operators/SetTiltAnglesOperator.h
  operators/SnapshotOperator.h
  operators/SnapshotOperator.cxx
  operators/TranslateAlignOperator.h
  operators/TranslateAlignOperator.cxx
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/operators)

set(exec_sources
  AboutDialog.h
  AboutDialog.cxx
  main.cxx
  MainWindow.cxx
  MainWindow.h
  WelcomeDialog.cxx
  WelcomeDialog.h
)

set(python_files
  AddPoissonNoise.py
  BinaryThreshold.py
  ConnectedComponents.py
  ClipEdges.py
  OtsuMultipleThreshold.py
  BinaryDilate.py
  BinaryErode.py
  BinaryOpen.py
  BinaryClose.py
  DummyMolecule.py
  LabelObjectAttributes.py
  LabelObjectPrincipalAxes.py
  LabelObjectDistanceFromPrincipalAxis.py
  AutoTiltAxisRotationAlignment.py
  AutoTiltAxisShiftAlignment.py
  AutoCenterOfMassTiltImageAlignment.py
  AutoCrossCorrelationTiltImageAlignment.py
  Recon_DFT.py
  Recon_DFT_constraint.py
  Recon_WBP.py
  Recon_ART.py
  Recon_SIRT.py
  Recon_TV_minimization.py
  FFT_AbsLog.py
  Shift_Stack_Uniformly.py
  Shift3D.py
  Square_Root_Data.py
  Subtract_TiltSer_Background.py
  Subtract_TiltSer_Background_Auto.py
  RemoveBadPixelsTiltSeries.py
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
  ShiftTiltSeriesRandomly.py
  STEM_probe.py
  InvertData.py
  Pad_Data.py
  BinVolumeByTwo.py
  BinTiltSeriesByTwo.py
  DefaultITKTransform.py
  BinaryMinMaxCurvatureFlow.py
  ReinterpretSignedToUnsigned.py
  SegmentParticles.py
  UnsharpMask.py
  AddConstant.py
  SegmentPores.py
  RotationAlign.py
  WienerFilter.py
  TV_Filter.py  
  )

set(json_files
  AddPoissonNoise.json
  AutoCenterOfMassTiltImageAlignment.json
  AutoCrossCorrelationTiltImageAlignment.json
  BinaryThreshold.json
  ConnectedComponents.json
  ClipEdges.json
  OtsuMultipleThreshold.json
  BinaryDilate.json
  BinaryErode.json
  BinaryOpen.json
  BinaryClose.json
  DummyMolecule.json
  LabelObjectAttributes.json
  LabelObjectPrincipalAxes.json
  LabelObjectDistanceFromPrincipalAxis.json
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
  Recon_DFT.json
  Recon_DFT_constraint.json
  Recon_TV_minimization.json
  Recon_SIRT.json
  Recon_WBP.json
  Shift3D.json
  ShiftTiltSeriesRandomly.json
  BinaryMinMaxCurvatureFlow.json
  SegmentParticles.json
  UnsharpMask.json
  AddConstant.json
  SegmentPores.json
  RotationAlign.json
  WienerFilter.json
  TV_Filter.json
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
  itkutils.py
  utils.py
  py2to3.py
  web.py
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

set(tomviz_python_io_modules
  __init__.py
  _internal.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/io")
foreach(file ${tomviz_python_io_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/${file}"
    "${tomviz_python_binary_dir}/tomviz/io/${file}")
endforeach()


# Autodiscover python reader/writers added to tomviz/python/tomviz/io/formats
file(GLOB tomviz_python_io_formats "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/formats/*.py")
file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/io/formats")
foreach(file_ ${tomviz_python_io_formats})
  get_filename_component(file ${file_} NAME)
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/formats/${file}"
    "${tomviz_python_binary_dir}/tomviz/io/formats/${file}")
endforeach()

# Install documentation (user guide)
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/docs")
execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
  "${tomviz_SOURCE_DIR}/docs/TomvizBasicUserGuide.pdf"
  "${tomviz_BINARY_DIR}/share/tomviz/docs/TomvizBasicUserGuide.pdf")
install(FILES "${tomviz_SOURCE_DIR}/docs/TomvizBasicUserGuide.pdf"
  DESTINATION "${tomviz_data_install_dir}/docs"
  COMPONENT "Documentation")

if(APPLE)
  list(APPEND exec_sources icons/tomviz.icns)
  set(MACOSX_BUNDLE_ICON_FILE tomviz.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${tomviz_version}")
  set_source_files_properties(icons/tomviz.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
  list(APPEND exec_sources icons/tomviz.rc)
endif()

include_directories(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

configure_file(tomvizConfig.h.in tomvizConfig.h @ONLY)
configure_file(tomvizPythonConfig.h.in tomvizPythonConfig.h @ONLY)

add_library(tomvizlib STATIC ${SOURCES} ${accel_srcs})
set_target_properties(tomvizlib PROPERTIES OUTPUT_NAME tomviz)

set_property(TARGET tomvizlib PROPERTY POSITION_INDEPENDENT_CODE TRUE)

add_executable(tomviz WIN32 MACOSX_BUNDLE ${exec_sources} resources.qrc)
target_link_libraries(tomviz PRIVATE tomvizlib ${OPENGL_LIBRARIES})

target_link_libraries(tomvizlib
  PUBLIC
    pqApplicationComponents
    vtkPVServerManagerRendering
    vtkglew
    vtkjsoncpp
    vtkpugixml
    tomvizExtensions
    Qt5::Network)
if(WIN32)
  target_link_libraries(tomvizlib PUBLIC Qt5::WinMain)
endif()
if(APPLE)
  set_target_properties(tomviz
    PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/tomviz.plist.in)
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

add_subdirectory(pybind11)

find_package(OpenGL REQUIRED)

include_directories(SYSTEM
  ${Qt6Widgets_INCLUDE_DIRS}
  ${Qt6Concurrent_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/thirdparty/pybind11/include)

add_subdirectory(core)
add_subdirectory(pvextensions)

set(SOURCES
  AbstractDataModel.h
  AbstractDataModel.cxx
  ActiveObjects.cxx
  ActiveObjects.h
  AddAlignReaction.cxx
  AddAlignReaction.h
  AddPythonSourceReaction.cxx
  AddPythonSourceReaction.h
  AddPythonTransformReaction.cxx
  AddRenderViewContextMenuBehavior.cxx
  AddRenderViewContextMenuBehavior.h
  AlignWidget.cxx
  AlignWidget.h
  AnimationHelperDialog.cxx
  AnimationHelperDialog.h
  AnimationSerializer.cxx
  AnimationSerializer.h
  ArrayWranglerReaction.cxx
  ArrayWranglerReaction.h
  AxesReaction.cxx
  AxesReaction.h
  Behaviors.cxx
  Behaviors.h
  BrightnessContrastWidget.cxx
  OpacityPresetWidget.cxx
  OpacityPresetWidget.h
  BrightnessContrastWidget.h
  CameraReaction.cxx
  CameraReaction.h
  CentralWidget.cxx
  CentralWidget.h
  CloneDataReaction.cxx
  CloneDataReaction.h
  ColorMap.cxx
  ColorMap.h
  ColorMapSettingsWidget.cxx
  ColorMapSettingsWidget.h
  ComboTextEditor.cxx
  ComboTextEditor.h
  ConvertToFloatReaction.cxx
  ConvertToFloatReaction.h
  CropReaction.cxx
  CropReaction.h
  CustomOperatorEditDialog.cxx
  CustomOperatorEditDialog.h
  CustomOperatorManagerDialog.cxx
  CustomOperatorManagerDialog.h
  SAM2SeedWidget.cxx
  SAM2SeedWidget.h
  SelectCylinderWidget.cxx
  SelectCylinderWidget.h
  SelectVolumeRangeWidget.cxx
  SelectVolumeRangeWidget.h
  ThresholdRangeWidget.cxx
  ThresholdRangeWidget.h
  LabelSelection.cxx
  LabelSelection.h
  LabelSelectionWidget.cxx
  LabelSelectionWidget.h
  SelectVolumeWidget.cxx
  SelectVolumeWidget.h
  DataBroker.cxx
  DataBroker.h
  DataBrokerLoadDialog.cxx
  DataBrokerLoadDialog.h
  DataBrokerLoadReaction.cxx
  DataBrokerLoadReaction.h
  DataBrokerSaveDialog.cxx
  DataBrokerSaveDialog.h
  DataBrokerSaveReaction.cxx
  DataBrokerSaveReaction.h
  DataExchangeFormat.cxx
  DataExchangeFormat.h
  DataPropertiesModel.cxx
  DataPropertiesModel.h
  DataTransformMenu.cxx
  DataTransformMenu.h
  DeleteDataReaction.cxx
  DeleteDataReaction.h
  DoubleSliderWidget.cxx
  DoubleSliderWidget.h
  DoubleSpinBox.cxx
  DoubleSpinBox.h
  EmdFormat.cxx
  EmdFormat.h
  FileFormatManager.cxx
  FileFormatManager.h
  FxiFormat.cxx
  FxiFormat.h
  ShiftRotationCenterWidget.cxx
  ShiftRotationCenterWidget.h
  GenericHDF5Format.cxx
  GenericHDF5Format.h
  GradientOpacityWidget.h
  GradientOpacityWidget.cxx
  Hdf5SubsampleWidget.h
  Hdf5SubsampleWidget.cxx
  HistogramManager.h
  HistogramManager.cxx
  HistogramWidget.h
  HistogramWidget.cxx
  Histogram2DWidget.h
  Histogram2DWidget.cxx
  ImageStackDialog.h
  ImageStackDialog.cxx
  ClippingRangeBehavior.cxx
  ClippingRangeBehavior.h
  ImageViewerModeBehavior.cxx
  ImageViewerModeBehavior.h
  ImageStackModel.h
  ImageStackModel.cxx
  IntSliderWidget.cxx
  IntSliderWidget.h
  ListEditorWidget.cxx
  ListEditorWidget.h
  LoadDataReaction.cxx
  LoadDataReaction.h
  LoadPaletteReaction.cxx
  LoadPaletteReaction.h
  LoadStackReaction.cxx
  LoadStackReaction.h
  LoadTimeSeriesReaction.cxx
  LoadTimeSeriesReaction.h
  Logger.cxx
  Logger.h
  ManualManipulationWidget.cxx
  ManualManipulationWidget.h
  OperatorSearchDialog.cxx
  OperatorSearchDialog.h
  MoleculeProperties.cxx
  MoleculeProperties.h
  MovieExportDialog.cxx
  MovieExportDialog.h
  InteractiveTransformWidget.cxx
  InteractiveTransformWidget.h
  PortDataWriter.cxx
  PortDataWriter.h
  PresetDialog.cxx
  PresetDialog.h
  PresetModel.cxx
  PresetModel.h
  ProgressDialog.cxx
  ProgressDialog.h
  ProgressDialogManager.cxx
  ProgressDialogManager.h
  PtychoWidget.cxx
  PtychoWidget.h
  PtychoRunner.cxx
  PtychoRunner.h
  PythonReader.cxx
  PythonReader.h
  PythonUtilities.cxx
  PythonUtilities.h
  PythonWriter.cxx
  PythonWriter.h
  PyXRFWidget.cxx
  PyXRFWidget.h
  PyXRFRunner.cxx
  PyXRFRunner.h
  pybind11/PythonTypeConversions.cxx
  pybind11/PythonTypeConversions.h
  QVTKGLWidget.cxx
  QVTKGLWidget.h
  RAWFileReaderDialog.h
  RAWFileReaderDialog.cxx
  Reaction.cxx
  Reaction.h
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
  SaveDataDialog.cxx
  SaveDataDialog.h
  SaveDataReaction.cxx
  SaveDataReaction.h
  SaveLoadStateReaction.cxx
  SaveLoadStateReaction.h
  SaveLoadTemplateReaction.cxx
  SaveLoadTemplateReaction.h
  SaveScreenshotDialog.cxx
  SaveScreenshotDialog.h
  SaveScreenshotReaction.cxx
  SaveScreenshotReaction.h
  SaveWebReaction.cxx
  ScaleActorBehavior.cxx
  ScaleActorBehavior.h
  SelectItemsDialog.cxx
  SelectItemsDialog.h
  SetDataTypeReaction.h
  SetDataTypeReaction.cxx
  SetTiltAnglesReaction.cxx
  SetTiltAnglesReaction.h
  SpinBox.cxx
  SpinBox.h
  TimeSeriesLabel.h
  TimeSeriesLabel.cxx
  TimeSeriesStep.h
  TomographyReconstruction.h
  TomographyReconstruction.cxx
  TomographyTiltSeries.h
  TomographyTiltSeries.cxx
  TrackpadTouchFix.h
  TrackpadTouchFix.cxx
  TransformUtils.h
  TransformUtils.cxx
  TransposeDataReaction.h
  TransposeDataReaction.cxx
  pipeline/transforms/CropTransform.h
  pipeline/transforms/CropTransform.cxx
  pipeline/transforms/ReconstructionTransform.h
  pipeline/transforms/ReconstructionTransform.cxx
  pipeline/transforms/TranslateAlignTransform.h
  pipeline/transforms/TranslateAlignTransform.cxx
  pipeline/PortData.cxx
  pipeline/PortData.h
  pipeline/PortDataDiskCache.cxx
  pipeline/PortDataDiskCache.h
  pipeline/PortDataMetadata.cxx
  pipeline/PortDataMetadata.h
  pipeline/PersistenceMode.h
  pipeline/PipelineSettings.cxx
  pipeline/PipelineSettings.h
  pipeline/PortType.h
  pipeline/NodeState.h
  pipeline/Port.cxx
  pipeline/Port.h
  pipeline/InputPort.cxx
  pipeline/InputPort.h
  pipeline/OutputPort.cxx
  pipeline/OutputPort.h
  pipeline/VolumeOutputPort.cxx
  pipeline/VolumeOutputPort.h
  pipeline/Link.cxx
  pipeline/Link.h
  pipeline/Node.cxx
  pipeline/Node.h
  pipeline/NodeFactory.cxx
  pipeline/NodeFactory.h
  pipeline/SourceNode.cxx
  pipeline/SourceNode.h
  pipeline/TransformNode.cxx
  pipeline/TransformNode.h
  pipeline/SinkNode.cxx
  pipeline/SinkNode.h
  pipeline/SinkGroupNode.cxx
  pipeline/SinkGroupNode.h
  pipeline/PassthroughOutputPort.cxx
  pipeline/PassthroughOutputPort.h
  pipeline/LegacyStateLoader.cxx
  pipeline/LegacyStateLoader.h
  pipeline/Pipeline.cxx
  pipeline/Pipeline.h
  pipeline/PipelineStateIO.cxx
  pipeline/PipelineStateIO.h
  pipeline/PipelineExecutor.cxx
  pipeline/PipelineExecutor.h
  pipeline/NodeExecutor.cxx
  pipeline/NodeExecutor.h
  pipeline/InternalNodeExecutor.cxx
  pipeline/InternalNodeExecutor.h
  pipeline/ExternalNodeExecutor.cxx
  pipeline/ExternalNodeExecutor.h
  pipeline/PythonEnvironmentCheck.cxx
  pipeline/PythonEnvironmentCheck.h
  pipeline/NodeExecutorFactory.cxx
  pipeline/NodeExecutorFactory.h
  pipeline/AutoExecuteController.cxx
  pipeline/AutoExecuteController.h
  pipeline/ProgressReader.cxx
  pipeline/ProgressReader.h
  pipeline/PythonNodeWrapper.cxx
  pipeline/PythonNodeWrapper.h
  pipeline/EnumOptions.cxx
  pipeline/EnumOptions.h
  pipeline/PythonNodeUtils.cxx
  pipeline/PythonNodeUtils.h
  pipeline/PythonNodeBackend.cxx
  pipeline/PythonNodeBackend.h
  pipeline/CustomNodeWidgetRegistry.cxx
  pipeline/CustomNodeWidgetRegistry.h
  pipeline/DefaultExecutor.cxx
  pipeline/DefaultExecutor.h
  pipeline/ExecutionFuture.cxx
  pipeline/ExecutionFuture.h
  pipeline/ThreadedExecutor.cxx
  pipeline/ThreadedExecutor.h
  pipeline/data/VolumeData.cxx
  pipeline/data/VolumeData.h
  pipeline/data/LabelMapData.cxx
  pipeline/data/LabelMapData.h
  pipeline/sources/SphereSource.cxx
  pipeline/sources/SphereSource.h
  pipeline/sources/ReaderSourceNode.cxx
  pipeline/sources/ReaderSourceNode.h
  pipeline/sources/PythonSource.cxx
  pipeline/sources/PythonSource.h
  pipeline/transforms/ThresholdTransform.cxx
  pipeline/transforms/ThresholdTransform.h
  pipeline/transforms/SetTiltAnglesTransform.cxx
  pipeline/transforms/SetTiltAnglesTransform.h
  pipeline/transforms/LegacyPythonTransform.cxx
  pipeline/transforms/LegacyPythonTransform.h
  pipeline/transforms/PythonTransform.cxx
  pipeline/transforms/PythonTransform.h
  pipeline/transforms/ConvertToFloatTransform.cxx
  pipeline/transforms/ConvertToFloatTransform.h
  pipeline/transforms/ConvertToVolumeTransform.cxx
  pipeline/transforms/ConvertToVolumeTransform.h
  pipeline/transforms/ArrayWranglerTransform.cxx
  pipeline/transforms/ArrayWranglerTransform.h
  pipeline/transforms/TransposeDataTransform.cxx
  pipeline/transforms/TransposeDataTransform.h
  pipeline/transforms/SnapshotTransform.cxx
  pipeline/transforms/SnapshotTransform.h
  pipeline/PortUtils.h
  pipeline/ThreadUtils.h
  pipeline/pybind11/PybindVTKTypeCaster.h
  pipeline/DeferredLinkInfo.h
  pipeline/sinks/VolumeStatsSink.cxx
  pipeline/sinks/VolumeStatsSink.h
  pipeline/sinks/LegacyModuleSink.cxx
  pipeline/sinks/LegacyModuleSink.h
  pipeline/sinks/ExplodedGeometry.cxx
  pipeline/sinks/ExplodedGeometry.h
  pipeline/sinks/VolumeSink.cxx
  pipeline/sinks/VolumeSink.h
  pipeline/sinks/LabelMapSink.cxx
  pipeline/sinks/LabelMapSink.h
  pipeline/sinks/MultiVolumeCoordinator.cxx
  pipeline/sinks/MultiVolumeCoordinator.h
  pipeline/sinks/LabelMapSurface.cxx
  pipeline/sinks/LabelMapSurface.h
  pipeline/sinks/LabelTableWidget.cxx
  pipeline/sinks/LabelTableWidget.h
  pipeline/sinks/LightingPresetStore.cxx
  pipeline/sinks/LightingPresetStore.h
  pipeline/sinks/VolumeBricking.cxx
  pipeline/sinks/VolumeBricking.h
  pipeline/sinks/SliceSink.cxx
  pipeline/sinks/SliceSink.h
  pipeline/sinks/ContourSink.cxx
  pipeline/sinks/ContourSink.h
  pipeline/sinks/ThresholdSink.cxx
  pipeline/sinks/ThresholdSink.h
  pipeline/sinks/SegmentSink.cxx
  pipeline/sinks/SegmentSink.h
  pipeline/sinks/OutlineSink.cxx
  pipeline/sinks/OutlineSink.h
  pipeline/sinks/ClipSink.cxx
  pipeline/sinks/ClipSink.h
  pipeline/sinks/RulerSink.cxx
  pipeline/sinks/RulerSink.h
  pipeline/sinks/ScaleCubeSink.cxx
  pipeline/sinks/ScaleCubeSink.h
  pipeline/sinks/PlotSink.cxx
  pipeline/sinks/PlotSink.h
  pipeline/sinks/MoleculeSink.cxx
  pipeline/sinks/MoleculeSink.h
  pipeline/sinks/VolumeSinkWidget.cxx
  pipeline/sinks/VolumeSinkWidget.h
  pipeline/sinks/ContourSinkWidget.cxx
  pipeline/sinks/ContourSinkWidget.h
  pipeline/sinks/ThresholdSinkWidget.cxx
  pipeline/sinks/ThresholdSinkWidget.h
  pipeline/sinks/SegmentSinkWidget.cxx
  pipeline/sinks/SegmentSinkWidget.h
  pipeline/ParameterBindingUtils.cxx
  pipeline/ParameterBindingUtils.h
  pipeline/ParameterInterfaceBuilder.cxx
  pipeline/ParameterInterfaceBuilder.h
  pipeline/CustomPythonNodeWidget.cxx
  pipeline/CustomPythonNodeWidget.h
  pipeline/InputsNotReadyWidget.cxx
  pipeline/InputsNotReadyWidget.h
  pipeline/GatedEditorWidget.cxx
  pipeline/GatedEditorWidget.h
  pipeline/EditNodeWidget.cxx
  pipeline/EditNodeWidget.h
  pipeline/NodePropertiesWidget.cxx
  pipeline/NodePropertiesWidget.h
  pipeline/NodeDefinitionValidator.cxx
  pipeline/NodeDefinitionValidator.h
  pipeline/NodeDefinitionEdits.cxx
  pipeline/NodeDefinitionEdits.h
  pipeline/NodeDefinitionFormWidget.cxx
  pipeline/NodeDefinitionFormWidget.h
  pipeline/NodeDefinitionWidget.cxx
  pipeline/NodeDefinitionWidget.h
  pipeline/PythonNodeEditorWidget.cxx
  pipeline/PythonNodeEditorWidget.h
  pipeline/PythonScriptEdit.cxx
  pipeline/PythonScriptEdit.h
  pipeline/NodePropertiesPanel.cxx
  pipeline/NodePropertiesPanel.h
  pipeline/NodeEditDialog.cxx
  pipeline/NodeEditDialog.h
  pipeline/PipelineUtils.cxx
  pipeline/PipelineUtils.h
  pipeline/PipelineControlsWidget.cxx
  pipeline/PipelineControlsWidget.h
  pipeline/PipelineStripWidget.cxx
  pipeline/PipelineStripWidget.h
  pipeline/LinkPropertiesWidget.cxx
  pipeline/LinkPropertiesWidget.h
  pipeline/SinkGroupPropertiesWidget.cxx
  pipeline/SinkGroupPropertiesWidget.h
  pipeline/VolumeScalarsModel.cxx
  pipeline/VolumeScalarsModel.h
  pipeline/VolumePropertiesWidget.cxx
  pipeline/VolumePropertiesWidget.h
  pipeline/vtkNonOrthoImagePlaneWidget.cxx
  pipeline/vtkNonOrthoImagePlaneWidget.h
  FileReader.cxx
  FileReader.h
  Tvh5Format.cxx
  Tvh5Format.h
  ViewsLayoutsSerializer.cxx
  ViewsLayoutsSerializer.h
  Utilities.cxx
  Utilities.h
  core/Variant.h
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
  vtkCustomBoxRepresentation.cxx
  vtkCustomBoxRepresentation.h
  vtkCustomPiecewiseControlPointsItem.cxx
  vtkCustomPiecewiseControlPointsItem.h
  vtkLengthScaleRepresentation.h
  vtkLengthScaleRepresentation.cxx
  vtkActiveScalarsProducer.h
  vtkActiveScalarsProducer.cxx
  vtkOMETiffReader.cxx
  vtkOMETiffReader.h
  vtkTriangleBar.cxx
  vtkTriangleBar.h
  WebExportWidget.cxx
  WebExportWidget.h
)

list(APPEND SOURCES
  loguru/loguru.cpp
  loguru/loguru.hpp
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/loguru)

# loguru is vendored third-party code; silence warnings we will not patch
# upstream for (empty variadic macro args and a benign snprintf truncation).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_source_files_properties(loguru/loguru.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-c++20-extensions;-Wno-format-truncation")
endif()

# acquisition/
list(APPEND SOURCES
  acquisition/AcquisitionWidget.cxx
  acquisition/AcquisitionWidget.h
  acquisition/AcquisitionClient.cxx
  acquisition/AcquisitionClient.h
  acquisition/Connection.cxx
  acquisition/Connection.h
  acquisition/ConnectionsWidget.cxx
  acquisition/ConnectionsWidget.h
  acquisition/ConnectionDialog.cxx
  acquisition/ConnectionDialog.h
  acquisition/JsonRpcClient.cxx
  acquisition/JsonRpcClient.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/acquisition)

list(APPEND SOURCES
  PipelineModuleMenu.cxx
  PipelineModuleMenu.h
)

# animations/
list(APPEND SOURCES
  animations/ModuleAnimation.h
  animations/ModuleAnimations.cxx
  animations/ModuleAnimations.h
  animations/OpacityInterpolation.cxx
  animations/OpacityInterpolation.h
  animations/ScalarOpacityAnimation.h
  animations/CameraAnimation.h
  animations/AnimationSceneGuard.cxx
  animations/CameraViewpoints.cxx
  animations/CameraViewpoints.h
  animations/RecordedAnimations.cxx
  animations/RecordedAnimations.h
  animations/SceneSnapshot.cxx
  animations/SceneSnapshot.h
  animations/AnimatableProperties.cxx
  animations/AnimatableProperties.h
  animations/ClipAnimation.h
  animations/ContourAnimation.h
  animations/CutOutAnimation.h
  animations/ExplodedAnimation.h
  animations/OpacityAnimation.h
  animations/SliceAnimation.h
  animations/SolidityAnimation.h
  animations/ThresholdAnimation.h
  animations/TimeSeriesAnimation.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/animations)

list(APPEND SOURCES
  h5cpp/h5readwrite.cpp
)

list(APPEND SOURCES
  AboutDialog.h
  AboutDialog.cxx
  MainWindow.cxx
  MainWindow.h
  WelcomeDialog.cxx
  WelcomeDialog.h
)

set(exec_sources
  main.cxx
)

set(python_files
  AddPoissonNoise.py
  BinaryThreshold.py
  CircleMask.py
  CombineDatasets.py
  FourierFilter.py
  FourierPeakMask.py
  FourierMask.py
  ImageMath.py
  ConnectedComponents.py
  RemoveLabels.py
  ClipEdges.py
  OtsuMultipleThreshold.py
  BinaryDilate.py
  BinaryErode.py
  BinaryOpen.py
  BinaryClose.py
  DummyMolecule.py
  DeconvolutionDenoise.py
  SimilarityMetrics.py
  ElastixRegistration.py
  LabelObjectAttributes.py
  LabelObjectPrincipalAxes.py
  LabelObjectDistanceFromPrincipalAxis.py
  AutoTiltAxisRotationAlignment.py
  AutoTiltAxisShiftAlignment.py
  AutoCenterOfMassTiltImageAlignment.py
  AutoCrossCorrelationTiltImageAlignment.py
  PyStackRegImageAlignment.py
  ctf_correct.py
  Recon_DFT.py
  Recon_DFT_constraint.py
  Recon_WBP.py
  Recon_ART.py
  Recon_SIRT.py
  Recon_TV_minimization.py
  Recon_tomopy.py
  ShiftRotationCenter_tomopy.py
  FFT_AbsLog.py
  ManualManipulation.py
  Shift_Stack_Uniformly.py
  Shift3D.py
  Square_Root_Data.py
  SwapAxes.py
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
  DeleteSlices.py
  ClearVolume.py
  ConstantDataset.py
  RandomParticles.py
  PtychoSource.py
  PyXRFSource.py
  GenerateTiltSeries.py
  SetNegativeVoxelsToZero.py
  ShiftTiltSeriesRandomly.py
  STEM_probe.py
  SimulatedLiveAcquisition.py
  InvertData.py
  Pad_Data.py
  BinVolumeByTwo.py
  BinTiltSeriesByTwo.py
  DefaultCustomTransform.py
  NewCustomTransform.py
  DefaultITKTransform.py
  BinaryMinMaxCurvatureFlow.py
  ReinterpretSignedToUnsigned.py
  SegmentParticles.py
  SAM2Segment3D.py
  SAM3Segment3D.py
  UnsharpMask.py
  AddConstant.py
  CylindricalCrop.py
  SegmentPores.py
  RotationAlign.py
  WienerFilter.py
  TV_Filter.py
  PoreSizeDistribution.py
  Tortuosity.py
  PowerSpectrumDensity.py
  FourierShellCorrelation.py
  RemoveArrays.py
  Recon_real_time_tomography.py
  )

set(json_files
  AddPoissonNoise.json
  NewCustomTransform.json
  AutoCenterOfMassTiltImageAlignment.json
  AutoCrossCorrelationTiltImageAlignment.json
  AutoTiltAxisRotationAlignment.json
  AutoTiltAxisShiftAlignment.json
  PyStackRegImageAlignment.json
  BinaryThreshold.json
  CircleMask.json
  CombineDatasets.json
  FourierFilter.json
  FourierPeakMask.json
  FourierMask.json
  ImageMath.json
  ConnectedComponents.json
  RemoveLabels.json
  ClipEdges.json
  ClearVolume.json
  OtsuMultipleThreshold.json
  BinaryDilate.json
  BinaryErode.json
  BinaryOpen.json
  BinaryClose.json
  DummyMolecule.json
  DeconvolutionDenoise.json
  SimilarityMetrics.json
  ElastixRegistration.json
  LabelObjectAttributes.json
  LabelObjectPrincipalAxes.json
  LabelObjectDistanceFromPrincipalAxis.json
  ManualManipulation.json
  Shift_Stack_Uniformly.json
  Pad_Data.json
  Resample.json
  DeleteSlices.json
  FFT_AbsLog.json
  GaussianFilter.json
  GaussianFilterTiltSeries.json
  PeronaMalikAnisotropicDiffusion.json
  MedianFilter.json
  Rotate3D.json
  GenerateTiltSeries.json
  ctf_correct.json
  Recon_ART.json
  Recon_DFT.json
  Recon_DFT_constraint.json
  Recon_TV_minimization.json
  Recon_tomopy.json
  ShiftRotationCenter_tomopy.json
  Recon_SIRT.json
  Recon_WBP.json
  Shift3D.json
  ShiftTiltSeriesRandomly.json
  SwapAxes.json
  BinaryMinMaxCurvatureFlow.json
  SegmentParticles.json
  SAM2Segment3D.json
  SAM3Segment3D.json
  UnsharpMask.json
  AddConstant.json
  CylindricalCrop.json
  SegmentPores.json
  RotationAlign.json
  WienerFilter.json
  TV_Filter.json
  PoreSizeDistribution.json
  Tortuosity.json
  PowerSpectrumDensity.json
  FourierShellCorrelation.json
  RemoveArrays.json
  Recon_real_time_tomography.json
  InvertData.json
  ConstantDataset.json
  RandomParticles.json
  PtychoSource.json
  PyXRFSource.json
  STEM_probe.json
  SimulatedLiveAcquisition.json
  )

set(resource_files
  defaultcolormaps.json
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
set(environment_files
  sam2-tomviz-cpu.yml
  sam2-tomviz-cuda.yml
  sam3-tomviz-cuda.yml
  )
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/environments")
foreach(file ${environment_files})
  list(APPEND SOURCES "python/environments/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/environments/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/environments/${file}")
  install(FILES "python/environments/${file}"
    DESTINATION "${tomviz_data_install_dir}/environments"
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

set(template_files
  Back_Projection_Reconstruction.tvsm
)

# Install templates
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/templates")
foreach(file ${template_files})
  list(APPEND SOURCES "templates/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/templates/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/templates/${file}")
  install(FILES "templates/${file}"
    DESTINATION "${tomviz_data_install_dir}"
    COMPONENT runtime)
endforeach()

# The generic pipeline engine lives in the external tomviz-pipeline
# package (import name tomviz_pipeline), installed alongside this
# package's Python dependencies. Only the app-side layer is staged here.
set(tomviz_python_modules
  __init__.py
  _boundary.py
  _internal.py
  _pipeline_requirement.py
  dataset.py
  fix_pdb.py
  nodes.py
  operators.py
  internal_utils.py
  itkutils.py
  utils.py
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

set(tomviz_real_time_files
  logger.py
  pytvlib.py
  wbp.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/_realtime")
foreach(file ${tomviz_real_time_files})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/_realtime/${file}"
    "${tomviz_python_binary_dir}/tomviz/_realtime/${file}")
endforeach()

set(tomviz_python_io_modules
  __init__.py
  _internal.py
  _databroker.py
  ser.py
  dm.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/io")
foreach(file ${tomviz_python_io_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/${file}"
    "${tomviz_python_binary_dir}/tomviz/io/${file}")
endforeach()

set(tomviz_python_pyxrf_modules
  __init__.py
  ic_names.py
  scan_metadata.py
  sids.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/pyxrf")
foreach(file ${tomviz_python_pyxrf_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/pyxrf/${file}"
    "${tomviz_python_binary_dir}/tomviz/pyxrf/${file}")
endforeach()

set(tomviz_python_ptycho_modules
  __init__.py
  ptycho.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/ptycho")
foreach(file ${tomviz_python_ptycho_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/ptycho/${file}"
    "${tomviz_python_binary_dir}/tomviz/ptycho/${file}")
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

if(APPLE AND TOMVIZ_MACOSX_BUNDLE)
  list(APPEND exec_sources icons/tomviz.icns)
  set(MACOSX_BUNDLE_ICON_FILE tomviz.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${tomviz_version}")
  # A non-empty bundle identifier and name are required for macOS to present
  # privacy-gated native panels (NSOpenPanel/NSSavePanel). Without them the
  # native QFileDialog silently fails to appear. Match the packaged bundle id.
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.tomviz.tomviz")
  set(MACOSX_BUNDLE_BUNDLE_NAME "tomviz")
  set_source_files_properties(icons/tomviz.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
  list(APPEND exec_sources icons/tomviz.rc)
endif()

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/core
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/core
  ${CMAKE_CURRENT_SOURCE_DIR}/pipeline)

# The tomviz-pipeline version this app depends on lives in
# python/tomviz/_pipeline_requirement.py (also read by setup.py) and is
# surfaced to the C++ external-environment check via tomvizConfig.h.
set(_pipeline_requirement_py
  "${CMAKE_CURRENT_SOURCE_DIR}/python/tomviz/_pipeline_requirement.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${_pipeline_requirement_py}")
file(STRINGS "${_pipeline_requirement_py}" _pipeline_requirement_line
  REGEX "^TOMVIZ_PIPELINE_MIN_VERSION[ ]*=[ ]*['\"]")
string(REGEX REPLACE
  "^TOMVIZ_PIPELINE_MIN_VERSION[ ]*=[ ]*['\"]([^'\"]+)['\"].*$" "\\1"
  tomviz_pipeline_min_version "${_pipeline_requirement_line}")
if(NOT tomviz_pipeline_min_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
  message(FATAL_ERROR
    "Could not read TOMVIZ_PIPELINE_MIN_VERSION from ${_pipeline_requirement_py}")
endif()
message(STATUS "Required tomviz-pipeline version: ${tomviz_pipeline_min_version}")

configure_file(tomvizConfig.h.in tomvizConfig.h @ONLY)
configure_file(tomvizPythonConfig.h.in tomvizPythonConfig.h @ONLY)

add_library(tomvizlib STATIC ${SOURCES} ${accel_srcs})
if(NOT WIN32)
  set_target_properties(tomvizlib PROPERTIES OUTPUT_NAME tomviz)
endif()
set_target_properties(tomvizlib PROPERTIES AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}/pipeline;${CMAKE_CURRENT_SOURCE_DIR}/pipeline/sinks")

# Need to set this property so that in stack traces, tomviz symbols
# can be resolved.
set_target_properties(tomvizlib PROPERTIES ENABLE_EXPORTS TRUE)

set_property(TARGET tomvizlib PROPERTY POSITION_INDEPENDENT_CODE TRUE)

find_package(Viskores QUIET)

if(APPLE AND TOMVIZ_MACOSX_BUNDLE)
  add_executable(tomviz MACOSX_BUNDLE ${exec_sources} resources.qrc pipeline/pipeline_icons.qrc)
else()
  add_executable(tomviz WIN32 ${exec_sources} resources.qrc pipeline/pipeline_icons.qrc)
endif()
target_link_libraries(tomviz PRIVATE tomvizlib ${OPENGL_LIBRARIES} ${extra_link_libraries})
if(Viskores_FOUND)
  target_link_libraries(tomviz PRIVATE viskores::cont)
endif()

# Need to set this property so that in stack traces, tomviz symbols
# can be resolved.
set_target_properties(tomviz PROPERTIES ENABLE_EXPORTS TRUE)

target_link_libraries(tomvizlib
  PUBLIC
    tomvizcore
    ParaView::pqApplicationComponents
    ParaView::pqPython
    ParaView::RemotingAnimation
    ParaView::RemotingExport
    ParaView::RemotingViews
    VTK::jsoncpp
    VTK::pugixml
    VTK::tiff
    VTK::CommonColor
    VTK::CommonCore
    VTK::CommonDataModel
    VTK::DomainsChemistry
    VTK::IOChemistry
    VTK::hdf5
    VTK::InteractionStyle
    VTK::RenderingVolume
    VTK::RenderingVolumeOpenGL2
    Qt6::Network
    Qt6::Widgets
  PRIVATE
    Qt6::Core5Compat
    Python3::Python
    $<$<PLATFORM_ID:Windows>:dbghelp shell32>
    pybind11::embed
    VTK::WrappingPythonCore
    VTK::IOImage
    VTK::IOXML
    VTK::IOLegacy
    VTK::RenderingCore
    VTK::FiltersCore
    VTK::FiltersSources
    VTK::FiltersGeneral
    VTK::FiltersModeling
    VTK::InteractionWidgets
    VTK::ImagingCore
    VTK::RenderingGridAxes
    VTK::ChartsCore
    VTK::ViewsContext2D)

if(APPLE AND TOMVIZ_MACOSX_BUNDLE)
  set_target_properties(tomviz
    PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/tomviz.plist.in)
  install(TARGETS tomviz DESTINATION Applications COMPONENT runtime)
else()
  install(TARGETS tomviz DESTINATION "${INSTALL_RUNTIME_DIR}" COMPONENT runtime)
endif()
install(TARGETS tomvizlib
  RUNTIME DESTINATION "${INSTALL_RUNTIME_DIR}"
  LIBRARY DESTINATION "${INSTALL_LIBRARY_DIR}"
  ARCHIVE DESTINATION "${INSTALL_ARCHIVE_DIR}")

if(tomviz_data_DIR)
  install(DIRECTORY "${tomviz_data_DIR}"
    DESTINATION "${tomviz_data_install_dir}"
    USE_SOURCE_PERMISSIONS
    COMPONENT runtime)
endif()

if(tomviz_templates_DIR)
  add_definitions(-DTOMVIZ_TEMPLATES)
  install(DIRECTORY "${tomviz_templates_DIR}"
    DESTINATION "${tomviz_data_install_dir}"
    USE_SOURCE_PERMISSIONS
    COMPONENT runtime)
endif()

add_subdirectory(pybind11)

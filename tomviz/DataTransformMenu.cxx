/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataTransformMenu.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>

#include "AddPythonTransformReaction.h"
#include "ArrayWranglerReaction.h"
#include "CloneDataReaction.h"
#include "ConvertToFloatReaction.h"
#include "CropReaction.h"
#include "DeleteDataReaction.h"
#include "TransposeDataReaction.h"
#include "Utilities.h"

namespace tomviz {

DataTransformMenu::DataTransformMenu(QMainWindow* mainWindow, QMenu* transform,
                                     QMenu* seg)
  : QObject(mainWindow), m_transformMenu(transform), m_segmentationMenu(seg),
    m_mainWindow(mainWindow)
{
  // Build the menu now
  buildTransforms();
  buildSegmentation();
}

void DataTransformMenu::buildTransforms()
{
  QMainWindow* mainWindow = m_mainWindow;
  QMenu* menu = m_transformMenu;
  menu->clear();

  auto customPythonAction = menu->addAction("Custom Transform");
  menu->addSeparator();

  // === Data Management submenu ===
  QMenu* dataManagement = menu->addMenu("Data Management");
  auto cropDataAction = dataManagement->addAction("Crop");
  auto cylindricalCropAction = dataManagement->addAction("Cylindrical Crop");
  auto convertDataAction = dataManagement->addAction("Convert to Float");
  auto arrayWranglerAction = dataManagement->addAction("Convert Type");
  auto transposeDataAction = dataManagement->addAction("Transpose Data");
  auto removeArraysAction = dataManagement->addAction("Remove Arrays");
  auto combineDatasetsAction = dataManagement->addAction("Combine Datasets");
  auto reinterpretSignedToUnignedAction =
    dataManagement->addAction("Reinterpret Signed to Unsigned");
  dataManagement->addSeparator();
  auto cloneAction = dataManagement->addAction("Clone");
  auto deleteDataAction = dataManagement->addAction(
    QIcon(":/QtWidgets/Icons/pqDelete.svg"), "Delete Data and Modules");
  deleteDataAction->setToolTip("Delete Data");

  // === Volume Manipulation submenu ===
  QMenu* volumeManip = menu->addMenu("Volume Manipulation");
  auto manualManipulationAction =
    volumeManip->addAction("Manual Manipulation");
  auto shiftUniformAction = volumeManip->addAction("Shift Volume");
  auto deleteSliceAction = volumeManip->addAction("Delete Slices");
  auto padVolumeAction = volumeManip->addAction("Pad Volume");
  auto downsampleByTwoAction = volumeManip->addAction("Bin Volume x2");
  auto resampleAction = volumeManip->addAction("Resample");
  auto rotateAction = volumeManip->addAction("Rotate");
  auto clearAction = volumeManip->addAction("Clear Subvolume");
  auto swapAction = volumeManip->addAction("Swap Axes");
  auto registrationAction = volumeManip->addAction("Registration");

  // === Math Operations submenu ===
  QMenu* mathOps = menu->addMenu("Math Operations");
  auto setNegativeVoxelsToZeroAction =
    mathOps->addAction("Set Negative Voxels To Zero");
  auto addConstantAction = mathOps->addAction("Add Constant");
  auto invertDataAction = mathOps->addAction("Invert Data");
  auto squareRootAction = mathOps->addAction("Square Root Data");
  auto cropEdgesAction = mathOps->addAction("Clip Edges");
  auto hannWindowAction = mathOps->addAction("Hann Window");
  auto fftAbsLogAction = mathOps->addAction("FFT (abs log)");
  auto fourierFilterAction = mathOps->addAction("Fourier Filter");
  auto fourierPeakMaskAction = mathOps->addAction("Fourier Peak Mask");
  auto fourierMaskAction = mathOps->addAction("Fourier Mask");
  auto imageMathAction = mathOps->addAction("Image Math");

  // === Filters & Smoothing submenu ===
  QMenu* filters = menu->addMenu("Filters && Smoothing");
  auto gradientMagnitudeSobelAction =
    filters->addAction("Gradient Magnitude");
  auto unsharpMaskAction = filters->addAction("Unsharp Mask");
  auto laplaceFilterAction = filters->addAction("Laplace Sharpen");
  auto gaussianFilterAction = filters->addAction("Gaussian Blur");
  auto wienerAction = filters->addAction("Wiener Filter");
  auto TVminAction =
    filters->addAction("Remove Stripes, Curtaining, Scratches");
  auto peronaMalikeAnisotropicDiffusionAction =
    filters->addAction("Perona-Malik Anisotropic Diffusion");
  auto medianFilterAction = filters->addAction("Median Filter");
  auto circleMaskAction = filters->addAction("Circle Mask");

  // === Material Analysis submenu ===
  QMenu* materialAnalysis = menu->addMenu("Material Analysis");
  auto tortuosityAction = materialAnalysis->addAction("Tortuosity");
  auto poreSizeAction =
    materialAnalysis->addAction("Pore Size Distribution");
  auto moleculeAction = materialAnalysis->addAction("Add Molecule");

  // === Metrics & Spectral submenu ===
  QMenu* metrics = menu->addMenu("Metrics && Spectral");
  auto psdAction = metrics->addAction("Power Spectrum Density");
  auto fscAction = metrics->addAction("Fourier Shell Correlation");
  auto deconvolutionDenoiseAction =
    metrics->addAction("Deconvolution Denoise");
  auto similarityMetricsAction = metrics->addAction("Similarity Metrics");

  // Add our Python script reactions, these compose Python into menu entries.
  new AddPythonTransformReaction(
    customPythonAction, "Custom Transform",
    readInPythonScript("DefaultCustomTransform"));
  new CropReaction(cropDataAction, mainWindow);
  new AddPythonTransformReaction(
    cylindricalCropAction, "Cylindrical Crop",
    readInPythonScript("CylindricalCrop"),
    readInJSONDescription("CylindricalCrop"));
  new ConvertToFloatReaction(convertDataAction);
  new ArrayWranglerReaction(arrayWranglerAction, mainWindow);
  new TransposeDataReaction(transposeDataAction, mainWindow);
  new AddPythonTransformReaction(
    removeArraysAction, "Remove Arrays",
    readInPythonScript("RemoveArrays"),
    readInJSONDescription("RemoveArrays"));
  new AddPythonTransformReaction(
    combineDatasetsAction, "Combine Datasets",
    readInPythonScript("CombineDatasets"),
    readInJSONDescription("CombineDatasets"));
  new AddPythonTransformReaction(
    reinterpretSignedToUnignedAction, "Reinterpret Signed to Unsigned",
    readInPythonScript("ReinterpretSignedToUnsigned"));

  new AddPythonTransformReaction(
    manualManipulationAction, "Manual Manipulation",
    readInPythonScript("ManualManipulation"),
    readInJSONDescription("ManualManipulation"));
  new AddPythonTransformReaction(
    shiftUniformAction, "Shift Volume",
    readInPythonScript("Shift_Stack_Uniformly"),
    readInJSONDescription("Shift_Stack_Uniformly"));
  new AddPythonTransformReaction(deleteSliceAction, "Delete Slices",
                                 readInPythonScript("DeleteSlices"),
                                 readInJSONDescription("DeleteSlices"));
  new AddPythonTransformReaction(padVolumeAction, "Pad Volume",
                                 readInPythonScript("Pad_Data"),
                                 readInJSONDescription("Pad_Data"));
  new AddPythonTransformReaction(downsampleByTwoAction, "Bin Volume x2",
                                 readInPythonScript("BinVolumeByTwo"));
  new AddPythonTransformReaction(resampleAction, "Resample",
                                 readInPythonScript("Resample"),
                                 readInJSONDescription("Resample"));
  new AddPythonTransformReaction(rotateAction, "Rotate",
                                 readInPythonScript("Rotate3D"),
                                 readInJSONDescription("Rotate3D"));
  new AddPythonTransformReaction(clearAction, "Clear Subvolume",
                                 readInPythonScript("ClearVolume"),
                                 readInJSONDescription("ClearVolume"));
  new AddPythonTransformReaction(swapAction, "Swap Axes",
                                 readInPythonScript("SwapAxes"),
                                 readInJSONDescription("SwapAxes"));
  new AddPythonTransformReaction(registrationAction, "Registration",
                                 readInPythonScript("ElastixRegistration"),
                                 readInJSONDescription("ElastixRegistration"));
  new AddPythonTransformReaction(setNegativeVoxelsToZeroAction,
                                 "Set Negative Voxels to Zero",
                                 readInPythonScript("SetNegativeVoxelsToZero"));
  new AddPythonTransformReaction(
    addConstantAction, "Add a Constant", readInPythonScript("AddConstant"),
    readInJSONDescription("AddConstant"));
  new AddPythonTransformReaction(invertDataAction, "Invert Data",
                                 readInPythonScript("InvertData"),
                                 readInJSONDescription("InvertData"));
  new AddPythonTransformReaction(squareRootAction, "Square Root Data",
                                 readInPythonScript("Square_Root_Data"));
  new AddPythonTransformReaction(cropEdgesAction, "Clip Edges",
                                 readInPythonScript("ClipEdges"),
                                 readInJSONDescription("ClipEdges"));
  new AddPythonTransformReaction(hannWindowAction, "Hann Window",
                                 readInPythonScript("HannWindow3D"));
  new AddPythonTransformReaction(fftAbsLogAction, "FFT (ABS LOG)",
                                 readInPythonScript("FFT_AbsLog"));
  new AddPythonTransformReaction(fourierFilterAction, "Fourier Filter",
                                 readInPythonScript("FourierFilter"),
                                 readInJSONDescription("FourierFilter"));
  new AddPythonTransformReaction(fourierPeakMaskAction, "Fourier Peak Mask",
                                 readInPythonScript("FourierPeakMask"),
                                 readInJSONDescription("FourierPeakMask"));
  new AddPythonTransformReaction(fourierMaskAction, "Fourier Mask",
                                 readInPythonScript("FourierMask"),
                                 readInJSONDescription("FourierMask"));
  new AddPythonTransformReaction(imageMathAction, "Image Math",
                                 readInPythonScript("ImageMath"),
                                 readInJSONDescription("ImageMath"));
  new AddPythonTransformReaction(gradientMagnitudeSobelAction,
                                 "Gradient Magnitude",
                                 readInPythonScript("GradientMagnitude_Sobel"));
  new AddPythonTransformReaction(
    unsharpMaskAction, "Unsharp Mask", readInPythonScript("UnsharpMask"),
    readInJSONDescription("UnsharpMask"));
  new AddPythonTransformReaction(laplaceFilterAction, "Laplace Sharpen",
                                 readInPythonScript("LaplaceFilter"));
  new AddPythonTransformReaction(
    wienerAction, "Wiener Filter", readInPythonScript("WienerFilter"),
    readInJSONDescription("WienerFilter"));
  new AddPythonTransformReaction(TVminAction, "TV_Filter",
                                 readInPythonScript("TV_Filter"),
                                 readInJSONDescription("TV_Filter"));
  new AddPythonTransformReaction(
    gaussianFilterAction, "Gaussian Blur", readInPythonScript("GaussianFilter"),
    readInJSONDescription("GaussianFilter"));
  new AddPythonTransformReaction(
    peronaMalikeAnisotropicDiffusionAction,
    "Perona-Malik Anisotropic Diffusion",
    readInPythonScript("PeronaMalikAnisotropicDiffusion"),
    readInJSONDescription("PeronaMalikAnisotropicDiffusion"));
  new AddPythonTransformReaction(
    medianFilterAction, "Median Filter", readInPythonScript("MedianFilter"),
    readInJSONDescription("MedianFilter"));
  new AddPythonTransformReaction(circleMaskAction, "Circle Mask",
                                 readInPythonScript("CircleMask"),
                                 readInJSONDescription("CircleMask"));
  new AddPythonTransformReaction(
    moleculeAction, "Add Molecule", readInPythonScript("DummyMolecule"),
    readInJSONDescription("DummyMolecule"));

  new AddPythonTransformReaction(
    tortuosityAction, "Tortuosity", readInPythonScript("Tortuosity"),
    readInJSONDescription("Tortuosity"));
  new AddPythonTransformReaction(
    poreSizeAction, "Pore Size Distribution",
    readInPythonScript("PoreSizeDistribution"),
    readInJSONDescription("PoreSizeDistribution"));

  new AddPythonTransformReaction(
    psdAction, "Power Spectrum Density",
    readInPythonScript("PowerSpectrumDensity"),
    readInJSONDescription("PowerSpectrumDensity"));
  new AddPythonTransformReaction(
    fscAction, "Fourier Shell Correlation",
    readInPythonScript("FourierShellCorrelation"),
    readInJSONDescription("FourierShellCorrelation"));
  new AddPythonTransformReaction(
    deconvolutionDenoiseAction, "Deconvolution Denoise",
    readInPythonScript("DeconvolutionDenoise"),
    readInJSONDescription("DeconvolutionDenoise"));
  new AddPythonTransformReaction(
    similarityMetricsAction, "Similarity Metrics",
    readInPythonScript("SimilarityMetrics"),
    readInJSONDescription("SimilarityMetrics"));

  new CloneDataReaction(cloneAction);
  new DeleteDataReaction(deleteDataAction);

}

void DataTransformMenu::buildSegmentation()
{
  QMenu* menu = m_segmentationMenu;
  menu->clear();

  auto customPythonITKAction = menu->addAction("Custom ITK Transform");

  // === Thresholding submenu ===
  QMenu* thresholding = menu->addMenu("Thresholding");
  auto binaryThresholdAction = thresholding->addAction("Binary Threshold");
  auto otsuMultipleThresholdAction =
    thresholding->addAction("Otsu Multiple Threshold");
  auto connectedComponentsAction =
    thresholding->addAction("Connected Components");
  auto removeLabelsAction = thresholding->addAction("Remove Labels");

  // === Morphology submenu ===
  QMenu* morphology = menu->addMenu("Morphology");
  auto binaryDilateAction = morphology->addAction("Binary Dilate");
  auto binaryErodeAction = morphology->addAction("Binary Erode");
  auto binaryOpenAction = morphology->addAction("Binary Open");
  auto binaryCloseAction = morphology->addAction("Binary Close");
  auto binaryMinMaxCurvatureFlowAction =
    morphology->addAction("Binary MinMax Curvature Flow");

  // === Label Analysis submenu ===
  QMenu* labelAnalysis = menu->addMenu("Label Analysis");
  auto labelObjectAttributesAction =
    labelAnalysis->addAction("Label Object Attributes");
  auto labelObjectPrincipalAxesAction =
    labelAnalysis->addAction("Label Object Principal Axes");
  auto distanceFromAxisAction =
    labelAnalysis->addAction("Label Object Distance From Principal Axis");

  // === Segmentation Workflows submenu ===
  QMenu* segWorkflows = menu->addMenu("Segmentation Workflows");
  auto segmentParticlesAction = segWorkflows->addAction("Segment Particles");
  auto segmentPoresAction = segWorkflows->addAction("Segment Pores");

  // === Machine Learning submenu ===
  QMenu* machineLearning = menu->addMenu("Machine Learning");
  auto sam2SegmentAction =
    machineLearning->addAction("SAM 2 Segmentation (3D)");
  auto sam3SegmentAction =
    machineLearning->addAction("SAM 3 Segmentation (3D)");

  new AddPythonTransformReaction(
    customPythonITKAction, "Custom ITK Transform",
    readInPythonScript("DefaultITKTransform"));
  new AddPythonTransformReaction(binaryThresholdAction, "Binary Threshold",
                                 readInPythonScript("BinaryThreshold"),
                                 readInJSONDescription("BinaryThreshold"));
  new AddPythonTransformReaction(
    otsuMultipleThresholdAction, "Otsu Multiple Threshold",
    readInPythonScript("OtsuMultipleThreshold"),
    readInJSONDescription("OtsuMultipleThreshold"));
  new AddPythonTransformReaction(
    connectedComponentsAction, "Connected Components",
    readInPythonScript("ConnectedComponents"),
    readInJSONDescription("ConnectedComponents"));
  new AddPythonTransformReaction(removeLabelsAction, "Remove Labels",
                                 readInPythonScript("RemoveLabels"),
                                 readInJSONDescription("RemoveLabels"));
  new AddPythonTransformReaction(
    binaryDilateAction, "Binary Dilate", readInPythonScript("BinaryDilate"),
    readInJSONDescription("BinaryDilate"));
  new AddPythonTransformReaction(
    binaryErodeAction, "Binary Erode", readInPythonScript("BinaryErode"),
    readInJSONDescription("BinaryErode"));
  new AddPythonTransformReaction(binaryOpenAction, "Binary Open",
                                 readInPythonScript("BinaryOpen"),
                                 readInJSONDescription("BinaryOpen"));
  new AddPythonTransformReaction(
    binaryCloseAction, "Binary Close", readInPythonScript("BinaryClose"),
    readInJSONDescription("BinaryClose"));
  new AddPythonTransformReaction(
    binaryMinMaxCurvatureFlowAction, "Binary MinMax Curvature Flow",
    readInPythonScript("BinaryMinMaxCurvatureFlow"),
    readInJSONDescription("BinaryMinMaxCurvatureFlow"));

  new AddPythonTransformReaction(
    labelObjectAttributesAction, "Label Object Attributes",
    readInPythonScript("LabelObjectAttributes"),
    readInJSONDescription("LabelObjectAttributes"));
  new AddPythonTransformReaction(
    labelObjectPrincipalAxesAction, "Label Object Principal Axes",
    readInPythonScript("LabelObjectPrincipalAxes"),
    readInJSONDescription("LabelObjectPrincipalAxes"));
  new AddPythonTransformReaction(
    distanceFromAxisAction, "Label Object Distance From Principal Axis",
    readInPythonScript("LabelObjectDistanceFromPrincipalAxis"),
    readInJSONDescription("LabelObjectDistanceFromPrincipalAxis"));

  new AddPythonTransformReaction(segmentParticlesAction, "Segment Particles",
                                 readInPythonScript("SegmentParticles"),
                                 readInJSONDescription("SegmentParticles"));
  new AddPythonTransformReaction(
    segmentPoresAction, "Segment Pores", readInPythonScript("SegmentPores"),
    readInJSONDescription("SegmentPores"));

  new AddPythonTransformReaction(
    sam2SegmentAction, "SAM 2 Segmentation (3D)",
    readInPythonScript("SAM2Segment3D"),
    readInJSONDescription("SAM2Segment3D"));
  new AddPythonTransformReaction(
    sam3SegmentAction, "SAM 3 Segmentation (3D)",
    readInPythonScript("SAM3Segment3D"),
    readInJSONDescription("SAM3Segment3D"));
}

void DataTransformMenu::updateActions() {}
} // namespace tomviz

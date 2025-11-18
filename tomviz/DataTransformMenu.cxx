/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataTransformMenu.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>

#include "AddExpressionReaction.h"
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

  // Build the Data Transforms menu
  auto customPythonAction = menu->addAction("Custom Transform");

  auto cropDataAction = menu->addAction("Crop");
  auto convertDataAction = menu->addAction("Convert to Float");
  auto arrayWranglerAction = menu->addAction("Convert Type");
  auto transposeDataAction = menu->addAction("Transpose Data");
  auto reinterpretSignedToUnignedAction =
    menu->addAction("Reinterpret Signed to Unsigned");
  menu->addSeparator();
  auto manualManipulationAction = menu->addAction("Manual Manipulation");
  auto shiftUniformAction = menu->addAction("Shift Volume");
  auto deleteSliceAction = menu->addAction("Delete Slices");
  auto padVolumeAction = menu->addAction("Pad Volume");
  auto downsampleByTwoAction = menu->addAction("Bin Volume x2");
  auto resampleAction = menu->addAction("Resample");
  auto rotateAction = menu->addAction("Rotate");
  auto clearAction = menu->addAction("Clear Subvolume");
  auto swapAction = menu->addAction("Swap Axes");
  auto registrationAction = menu->addAction("Registration");
  menu->addSeparator();
  auto setNegativeVoxelsToZeroAction =
    menu->addAction("Set Negative Voxels To Zero");
  auto addConstantAction = menu->addAction("Add Constant");
  auto invertDataAction = menu->addAction("Invert Data");
  auto squareRootAction = menu->addAction("Square Root Data");
  auto cropEdgesAction = menu->addAction("Clip Edges");
  auto hannWindowAction = menu->addAction("Hann Window");
  auto fftAbsLogAction = menu->addAction("FFT (abs log)");
  auto gradientMagnitudeSobelAction = menu->addAction("Gradient Magnitude");
  auto unsharpMaskAction = menu->addAction("Unsharp Mask");
  auto laplaceFilterAction = menu->addAction("Laplace Sharpen");
  auto gaussianFilterAction = menu->addAction("Gaussian Blur");
  auto wienerAction = menu->addAction("Wiener Filter");
  auto TVminAction = menu->addAction("Remove Stripes, Curtaining, Scratches");
  auto peronaMalikeAnisotropicDiffusionAction =
    menu->addAction("Perona-Malik Anisotropic Diffusion");
  auto medianFilterAction = menu->addAction("Median Filter");
  auto circleMaskAction = menu->addAction("Circle Mask");
  auto moleculeAction = menu->addAction("Add Molecule");
  menu->addSeparator();
  auto tortuosityAction = menu->addAction("Tortuosity");
  auto poreSizeAction = menu->addAction("Pore Size Distribution");
  menu->addSeparator();
  auto cloneAction = menu->addAction("Clone");
  auto deleteDataAction = menu->addAction(
    QIcon(":/QtWidgets/Icons/pqDelete.svg"), "Delete Data and Modules");
  deleteDataAction->setToolTip("Delete Data");

  // Add our Python script reactions, these compose Python into menu entries.
  new AddExpressionReaction(customPythonAction);
  new CropReaction(cropDataAction, mainWindow);
  new ConvertToFloatReaction(convertDataAction);
  new ArrayWranglerReaction(arrayWranglerAction, mainWindow);
  new TransposeDataReaction(transposeDataAction, mainWindow);
  new AddPythonTransformReaction(
    reinterpretSignedToUnignedAction, "Reinterpret Signed to Unsigned",
    readInPythonScript("ReinterpretSignedToUnsigned"));

  new AddPythonTransformReaction(
    manualManipulationAction, "Manual Manipulation",
    readInPythonScript("ManualManipulation"), false, false, false,
    readInJSONDescription("ManualManipulation"));
  new AddPythonTransformReaction(
    shiftUniformAction, "Shift Volume",
    readInPythonScript("Shift_Stack_Uniformly"), false, false, false,
    readInJSONDescription("Shift_Stack_Uniformly"));
  new AddPythonTransformReaction(deleteSliceAction, "Delete Slices",
                                 readInPythonScript("DeleteSlices"),
                                 false, false, false,
                                 readInJSONDescription("DeleteSlices"));
  new AddPythonTransformReaction(padVolumeAction, "Pad Volume",
                                 readInPythonScript("Pad_Data"), false, false,
                                 false, readInJSONDescription("Pad_Data"));
  new AddPythonTransformReaction(downsampleByTwoAction, "Bin Volume x2",
                                 readInPythonScript("BinVolumeByTwo"));
  new AddPythonTransformReaction(resampleAction, "Resample",
                                 readInPythonScript("Resample"), false, false,
                                 false, readInJSONDescription("Resample"));
  new AddPythonTransformReaction(rotateAction, "Rotate",
                                 readInPythonScript("Rotate3D"), false, false,
                                 false, readInJSONDescription("Rotate3D"));
  new AddPythonTransformReaction(clearAction, "Clear Volume",
                                 readInPythonScript("ClearVolume"));
  new AddPythonTransformReaction(swapAction, "Swap Axes",
                                 readInPythonScript("SwapAxes"), false, false,
                                 false, readInJSONDescription("SwapAxes"));
  new AddPythonTransformReaction(registrationAction, "Registration",
                                 readInPythonScript("ElastixRegistration"),
                                 false, false, false,
                                 readInJSONDescription("ElastixRegistration"));
  new AddPythonTransformReaction(setNegativeVoxelsToZeroAction,
                                 "Set Negative Voxels to Zero",
                                 readInPythonScript("SetNegativeVoxelsToZero"));
  new AddPythonTransformReaction(
    addConstantAction, "Add a Constant", readInPythonScript("AddConstant"),
    false, false, false, readInJSONDescription("AddConstant"));
  new AddPythonTransformReaction(invertDataAction, "Invert Data",
                                 readInPythonScript("InvertData"));
  new AddPythonTransformReaction(squareRootAction, "Square Root Data",
                                 readInPythonScript("Square_Root_Data"));
  new AddPythonTransformReaction(cropEdgesAction, "Clip Edges",
                                 readInPythonScript("ClipEdges"), false, true,
                                 false, readInJSONDescription("ClipEdges"));
  new AddPythonTransformReaction(hannWindowAction, "Hann Window",
                                 readInPythonScript("HannWindow3D"));
  new AddPythonTransformReaction(fftAbsLogAction, "FFT (ABS LOG)",
                                 readInPythonScript("FFT_AbsLog"));
  new AddPythonTransformReaction(gradientMagnitudeSobelAction,
                                 "Gradient Magnitude",
                                 readInPythonScript("GradientMagnitude_Sobel"));
  new AddPythonTransformReaction(
    unsharpMaskAction, "Unsharp Mask", readInPythonScript("UnsharpMask"), false,
    false, false, readInJSONDescription("UnsharpMask"));
  new AddPythonTransformReaction(laplaceFilterAction, "Laplace Sharpen",
                                 readInPythonScript("LaplaceFilter"));
  new AddPythonTransformReaction(
    wienerAction, "Wiener Filter", readInPythonScript("WienerFilter"), false,
    false, false, readInJSONDescription("WienerFilter"));
  new AddPythonTransformReaction(TVminAction, "TV_Filter",
                                 readInPythonScript("TV_Filter"), false, false,
                                 false, readInJSONDescription("TV_Filter"));
  new AddPythonTransformReaction(
    gaussianFilterAction, "Gaussian Blur", readInPythonScript("GaussianFilter"),
    false, false, false, readInJSONDescription("GaussianFilter"));
  new AddPythonTransformReaction(
    peronaMalikeAnisotropicDiffusionAction,
    "Perona-Malik Anisotropic Diffusion",
    readInPythonScript("PeronaMalikAnisotropicDiffusion"), false, false, false,
    readInJSONDescription("PeronaMalikAnisotropicDiffusion"));
  new AddPythonTransformReaction(
    medianFilterAction, "Median Filter", readInPythonScript("MedianFilter"),
    false, false, false, readInJSONDescription("MedianFilter"));
  new AddPythonTransformReaction(circleMaskAction, "Circle Mask",
                                 readInPythonScript("CircleMask"), false, false,
                                 false, readInJSONDescription("CircleMask"));
  new AddPythonTransformReaction(
    moleculeAction, "Add Molecule", readInPythonScript("DummyMolecule"), false,
    false, false, readInJSONDescription("DummyMolecule"));

  new AddPythonTransformReaction(
    tortuosityAction, "Tortuosity", readInPythonScript("Tortuosity"), false,
    false, false, readInJSONDescription("Tortuosity"));
  new AddPythonTransformReaction(
    poreSizeAction, "Pore Size Distribution",
    readInPythonScript("PoreSizeDistribution"), false, false, false,
    readInJSONDescription("PoreSizeDistribution"));

  new CloneDataReaction(cloneAction);
  new DeleteDataReaction(deleteDataAction);

  // TODO - enable/disable menu actions depending on whether the selected
  // DataSource has
  // the properties required by the Data Transform
}

void DataTransformMenu::buildSegmentation()
{
  QMenu* menu = m_segmentationMenu;
  menu->clear();

  auto customPythonITKAction = menu->addAction("Custom ITK Transform");
  menu->addSeparator();
  auto binaryThresholdAction = menu->addAction("Binary Threshold");
  auto otsuMultipleThresholdAction = menu->addAction("Otsu Multiple Threshold");
  auto connectedComponentsAction = menu->addAction("Connected Components");
  menu->addSeparator();
  auto binaryDilateAction = menu->addAction("Binary Dilate");
  auto binaryErodeAction = menu->addAction("Binary Erode");
  auto binaryOpenAction = menu->addAction("Binary Open");
  auto binaryCloseAction = menu->addAction("Binary Close");
  auto binaryMinMaxCurvatureFlowAction =
    menu->addAction("Binary MinMax Curvature Flow");
  menu->addSeparator();
  auto labelObjectAttributesAction = menu->addAction("Label Object Attributes");
  auto labelObjectPrincipalAxesAction =
    menu->addAction("Label Object Principal Axes");
  auto distanceFromAxisAction =
    menu->addAction("Label Object Distance From Principal Axis");
  menu->addSeparator();
  auto segmentParticlesAction = menu->addAction("Segment Particles");
  auto segmentPoresAction = menu->addAction("Segment Pores");

  new AddExpressionReaction(customPythonITKAction);
  new AddPythonTransformReaction(binaryThresholdAction, "Binary Threshold",
                                 readInPythonScript("BinaryThreshold"), false,
                                 false, false,
                                 readInJSONDescription("BinaryThreshold"));
  new AddPythonTransformReaction(
    otsuMultipleThresholdAction, "Otsu Multiple Threshold",
    readInPythonScript("OtsuMultipleThreshold"), false, false, false,
    readInJSONDescription("OtsuMultipleThreshold"));
  new AddPythonTransformReaction(
    connectedComponentsAction, "Connected Components",
    readInPythonScript("ConnectedComponents"), false, false, false,
    readInJSONDescription("ConnectedComponents"));
  new AddPythonTransformReaction(
    binaryDilateAction, "Binary Dilate", readInPythonScript("BinaryDilate"),
    false, false, false, readInJSONDescription("BinaryDilate"));
  new AddPythonTransformReaction(
    binaryErodeAction, "Binary Erode", readInPythonScript("BinaryErode"), false,
    false, false, readInJSONDescription("BinaryErode"));
  new AddPythonTransformReaction(binaryOpenAction, "Binary Open",
                                 readInPythonScript("BinaryOpen"), false, false,
                                 false, readInJSONDescription("BinaryOpen"));
  new AddPythonTransformReaction(
    binaryCloseAction, "Binary Close", readInPythonScript("BinaryClose"), false,
    false, false, readInJSONDescription("BinaryClose"));
  new AddPythonTransformReaction(
    binaryMinMaxCurvatureFlowAction, "Binary MinMax Curvature Flow",
    readInPythonScript("BinaryMinMaxCurvatureFlow"), false, false, false,
    readInJSONDescription("BinaryMinMaxCurvatureFlow"));

  new AddPythonTransformReaction(
    labelObjectAttributesAction, "Label Object Attributes",
    readInPythonScript("LabelObjectAttributes"), false, false, false,
    readInJSONDescription("LabelObjectAttributes"));
  new AddPythonTransformReaction(
    labelObjectPrincipalAxesAction, "Label Object Principal Axes",
    readInPythonScript("LabelObjectPrincipalAxes"), false, false, false,
    readInJSONDescription("LabelObjectPrincipalAxes"));
  new AddPythonTransformReaction(
    distanceFromAxisAction, "Label Object Distance From Principal Axis",
    readInPythonScript("LabelObjectDistanceFromPrincipalAxis"), false, false,
    false, readInJSONDescription("LabelObjectDistanceFromPrincipalAxis"));

  new AddPythonTransformReaction(segmentParticlesAction, "Segment Particles",
                                 readInPythonScript("SegmentParticles"), false,
                                 false, false,
                                 readInJSONDescription("SegmentParticles"));
  new AddPythonTransformReaction(
    segmentPoresAction, "Segment Pores", readInPythonScript("SegmentPores"),
    false, false, false, readInJSONDescription("SegmentPores"));
}

void DataTransformMenu::updateActions() {}
} // namespace tomviz

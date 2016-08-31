/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "DataTransformMenu.h"

#include <QAction>
#include <QMainWindow>
#include <QMenu>

#include "AddExpressionReaction.h"
#include "AddPythonTransformReaction.h"
#include "CloneDataReaction.h"
#include "ConvertToFloatReaction.h"
#include "CropReaction.h"
#include "DeleteDataReaction.h"
#include "Utilities.h"

namespace tomviz {

class DataTransformMenu::DTInternals
{
public:
  DTInternals(QMainWindow* mainWindow, QMenu* menu)
    : MainWindow(mainWindow), Menu(menu)
  {
  }
  ~DTInternals() {}

  QMainWindow* MainWindow;
  QMenu* Menu;
};

DataTransformMenu::DataTransformMenu(QMainWindow* mainWindow, QMenu* menu)
  : Superclass(mainWindow), Internals(new DTInternals(mainWindow, menu))
{
  // Build the menu now
  this->buildMenu();

  // Set up the menu to rebuild every mouse click. This gives us a chance
  // to update the enabled/disabled state of each menu item.
  QObject::connect(menu, SIGNAL(aboutToShow()), this, SLOT(buildMenu()));
}

DataTransformMenu::~DataTransformMenu()
{
}

void DataTransformMenu::buildMenu()
{
  QMainWindow* mainWindow = this->Internals->MainWindow;
  QMenu* menu = this->Internals->Menu;
  menu->clear();

  // Build the Data Transforms menu
  QAction* customPythonAction = menu->addAction("Custom Transform");

  QAction* cropDataAction = menu->addAction("Crop");
  QAction* convertDataAction = menu->addAction("Convert To Float");
  menu->addSeparator();
  QAction* customPythonITKAction = menu->addAction("Custom ITK Transform");
  QAction* binaryThresholdAction = menu->addAction("Binary Threshold");
  QAction* connectedComponentsAction = menu->addAction("Connected Components");
  menu->addSeparator();
  QAction* shiftUniformAction = menu->addAction("Shift Volume");
  QAction* deleteSliceAction = menu->addAction("Delete Slices");
  QAction* padVolumeAction = menu->addAction("Pad Volume");
  QAction* downsampleByTwoAction = menu->addAction("Bin Volume x2");
  QAction* resampleAction = menu->addAction("Resample");
  QAction* rotateAction = menu->addAction("Rotate");
  QAction* clearAction = menu->addAction("Clear Subvolume");
  menu->addSeparator();
  QAction* setNegativeVoxelsToZeroAction =
    menu->addAction("Set Negative Voxels To Zero");
  QAction* invertDataAction = menu->addAction("Invert Data");
  QAction* squareRootAction = menu->addAction("Square Root Data");
  QAction* hannWindowAction = menu->addAction("Hann Window");
  QAction* fftAbsLogAction = menu->addAction("FFT (abs log)");
  QAction* gradientMagnitudeSobelAction = menu->addAction("Gradient Magnitude");
  QAction* laplaceFilterAction = menu->addAction("Laplace Filter");
  QAction* gaussianFilterAction = menu->addAction("Gaussian Filter");
  QAction* medianFilterAction = menu->addAction("Median Filter");
  menu->addSeparator();

  QAction* cloneAction = menu->addAction("Clone");
  QAction* deleteDataAction = menu->addAction(
    QIcon(":/QtWidgets/Icons/pqDelete32.png"), "Delete Data and Modules");
  deleteDataAction->setToolTip("Delete Data");

  // Add our Python script reactions, these compose Python into menu entries.
  new AddExpressionReaction(customPythonAction);
  new AddExpressionReaction(customPythonITKAction);
  new CropReaction(cropDataAction, mainWindow);
  new ConvertToFloatReaction(convertDataAction);
  new AddPythonTransformReaction(binaryThresholdAction, "Binary Threshold",
                                 readInPythonScript("BinaryThreshold"), false,
                                 false,
                                 readInJSONDescription("BinaryThreshold"));
  new AddPythonTransformReaction(
    connectedComponentsAction, "Connected Components",
    readInPythonScript("ConnectedComponents"), false, false,
    readInJSONDescription("ConnectedComponents"));
  new AddPythonTransformReaction(shiftUniformAction, "Shift Volume",
                                 readInPythonScript("Shift_Stack_Uniformly"));
  new AddPythonTransformReaction(deleteSliceAction, "Delete Slices",
                                 readInPythonScript("deleteSlices"));
  new AddPythonTransformReaction(padVolumeAction, "Pad Volume",
                                 readInPythonScript("Pad_Data"));
  new AddPythonTransformReaction(downsampleByTwoAction, "Bin Volume x2",
                                 readInPythonScript("BinVolumeByTwo"));
  new AddPythonTransformReaction(resampleAction, "Resample",
                                 readInPythonScript("Resample"));
  new AddPythonTransformReaction(rotateAction, "Rotate",
                                 readInPythonScript("Rotate3D"));
  new AddPythonTransformReaction(clearAction, "Clear Volume",
                                 readInPythonScript("ClearVolume"));
  new AddPythonTransformReaction(setNegativeVoxelsToZeroAction,
                                 "Set Negative Voxels to Zero",
                                 readInPythonScript("SetNegativeVoxelsToZero"));
  new AddPythonTransformReaction(invertDataAction, "Invert Data",
                                 readInPythonScript("InvertData"));
  new AddPythonTransformReaction(squareRootAction, "Square Root Data",
                                 readInPythonScript("Square_Root_Data"));
  new AddPythonTransformReaction(hannWindowAction, "Hann Window",
                                 readInPythonScript("HannWindow3D"));
  new AddPythonTransformReaction(fftAbsLogAction, "FFT (ABS LOG)",
                                 readInPythonScript("FFT_AbsLog"));
  new AddPythonTransformReaction(gradientMagnitudeSobelAction,
                                 "Gradient Magnitude",
                                 readInPythonScript("GradientMagnitude_Sobel"));
  new AddPythonTransformReaction(laplaceFilterAction, "Laplace Filter",
                                 readInPythonScript("LaplaceFilter"));
  new AddPythonTransformReaction(gaussianFilterAction, "Gaussian Filter",
                                 readInPythonScript("GaussianFilter"));
  new AddPythonTransformReaction(medianFilterAction, "Median Filter",
                                 readInPythonScript("MedianFilter"));

  new CloneDataReaction(cloneAction);
  new DeleteDataReaction(deleteDataAction);

  // TODO - enable/disable menu actions depending on whether the selected
  // DataSource has
  // the properties required by the Data Transform
}

void DataTransformMenu::updateActions()
{
}
}

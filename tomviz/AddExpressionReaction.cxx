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
#include "AddExpressionReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "OperatorPython.h"
#include "EditOperatorDialog.h"

#include <pqCoreUtilities.h>

namespace tomviz
{

AddExpressionReaction::AddExpressionReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  this->connect(&ActiveObjects::instance(),
                SIGNAL(dataSourceChanged(DataSource*)),
                SLOT(updateEnableState()));
  this->updateEnableState();
}

AddExpressionReaction::~AddExpressionReaction()
{
}

void AddExpressionReaction::updateEnableState()
{
  this->parentAction()->setEnabled(
    ActiveObjects::instance().activeDataSource() != nullptr);
}

OperatorPython* AddExpressionReaction::addExpression(DataSource* source)
{
  source = source ? source : ActiveObjects::instance().activeDataSource();
  if (!source)
  {
    return nullptr;
  }

  QString script = this->getDefaultExpression(source);

  OperatorPython *opPython = new OperatorPython();
  opPython->setScript(script);
  QSharedPointer<Operator> op(opPython);
  opPython->setLabel("Transform Data");

  // Create a non-modal dialog, delete it once it has been closed.
  EditOperatorDialog *dialog =
      new EditOperatorDialog(op, source, pqCoreUtilities::mainWidget());
  dialog->setAttribute(Qt::WA_DeleteOnClose, true);
  dialog->show();
  return nullptr;
}

QString AddExpressionReaction::getDefaultExpression(DataSource *source)
{
  QString actionString = this->parentAction()->text();
  if (actionString == "Custom ITK Transform")
  {
    return QString(
      "def transform_scalars(dataset):\n"
      "    \"\"\"Define this method for Python operators that \n"
      "    transform the input array\"\"\"\n"
      "\n"
      "    from tomviz import utils\n"
      "    import numpy as np\n"
      "    import itk\n"
      "\n"
      "    # Get the current volume as a numpy array.\n"
      "    array = utils.get_array(dataset)\n"
      "\n"
      "    # Set up some ITK variables\n"
      "    itk_image_type = itk.Image.F3\n"
      "    itk_converter = itk.PyBuffer[itk_image_type]\n"
      "\n"
      "    # Read the image into ITK\n"
      "    itk_image = itk_converter.GetImageFromArray(array)\n"
      "\n"
      "    # ITK filter (I have no idea if this is right)\n"
      "    filter = itk.ConfidenceConnectedImageFilter[itk_image_type,itk.Image.SS3].New()\n"
      "    filter.SetInitialNeighborhoodRadius(3)\n"
      "    filter.SetMultiplier(3)\n"
      "    filter.SetNumberOfIterations(25)\n"
      "    filter.SetReplaceValue(255)\n"
      "    filter.SetSeed((24,65,37))\n"
      "    filter.SetInput(itk_image)\n"
      "    filter.Update()\n"
      "\n"
      "    # Get the image back from ITK (result is a numpy image)\n"
      "    result = itk.PyBuffer[itk.Image.SS3].GetArrayFromImage(filter.GetOutput())\n"
      "\n"
      "    # This is where the transformed data is set, it will display in tomviz.\n"
      "    utils.set_array(dataset, result)\n");
  }
  else
  {
  // Build the default script for the python operator
  // This was done in the Dialog's UI file, but since it needs to change
  // based on the type of dataset, do it here
  return QString("def transform_scalars(dataset):\n"
      "    \"\"\"Define this method for Python operators that \n"
      "    transform the input array\"\"\"\n"
      "\n"
      "    from tomviz import utils\n"
      "    import numpy as np\n"
      "\n"
      "%1"
      "    # Get the current volume as a numpy array.\n"
      "    array = utils.get_array(dataset)\n"
      "\n"
      "    # This is where you operate on your data, here we square root it.\n"
      "    result = np.sqrt(array)\n"
      "\n" 
      "    # This is where the transformed data is set, it will display in tomviz.\n"
      "    utils.set_array(dataset, result)\n")
      .arg( source->type() == DataSource::Volume ? "" :
      "    # Get the tilt angles array as a numpy array.\n"
      "    # There is also a utils.set_tilt_angles(dataset, angles) function if you need\n"
      "    # to set tilt angles.\n"
      "    tilt_angles = utils.get_tilt_angles(dataset)\n"
      "\n");
  }
}
}

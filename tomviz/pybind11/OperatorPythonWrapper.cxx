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

#include "OperatorPythonWrapper.h"
#include "OperatorPython.h"

#include <vtkSmartPointer.h>
#include <vtkImageData.h>

using namespace tomviz;

OperatorPythonWrapper::OperatorPythonWrapper(void* o)
{
  this->op = static_cast<OperatorPython*>(o);
}

bool OperatorPythonWrapper::canceled()
{
  return this->op->isCanceled();
}

void OperatorPythonWrapper::setTotalProgressSteps(int progress)
{
  this->op->setTotalProgressSteps(progress);
}

int OperatorPythonWrapper::totalProgressSteps()
{
  return this->op->totalProgressSteps();
}

void OperatorPythonWrapper::setProgressStep(int progress)
{
  this->op->setProgressStep(progress);
}

int OperatorPythonWrapper::progressStep()
{
  return this->op->progressStep();
}

void OperatorPythonWrapper::setProgressMessage(const std::string& message)
{
  QString msg = QString::fromStdString(message);
  this->op->setProgressMessage(msg);
}

std::string OperatorPythonWrapper::progressMessage()
{
  return this->op->progressMessage().toStdString();
}

void OperatorPythonWrapper::progressData()
{
}

void OperatorPythonWrapper::setProgressData(vtkImageData* imageData)
{
  emit this->op->childDataSourceUpdated(imageData);
}


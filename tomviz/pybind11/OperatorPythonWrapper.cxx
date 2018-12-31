/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorPythonWrapper.h"
#include "OperatorPython.h"
#include "PythonUtilities.h"

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

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

void OperatorPythonWrapper::progressData() {}

void OperatorPythonWrapper::setProgressData(vtkImageData* imageData)
{
  TemporarilyReleaseGil releaseMe;
  emit this->op->childDataSourceUpdated(imageData);
}

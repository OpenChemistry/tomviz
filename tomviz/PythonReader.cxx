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
#include "PythonReader.h"

#include "DataSource.h"

#include <vtkImageData.h>

#include <QDebug>

namespace tomviz {

PythonReader::PythonReader(Python::Object instance) : m_instance(instance)
{
}

vtkSmartPointer<vtkImageData> PythonReader::read(QString fileName)
{
  Python python;
  auto module = python.import("tomviz.io._internal");
  if (!module.isValid()) {
    qCritical() << "Failed to import tomviz.io._internal module.";
    return nullptr;
  }
  auto readerFunction = module.findFunction("execute_reader");
  if (!readerFunction.isValid()) {
    qCritical() << "Failed to import tomviz.io._internal.execute_reader";
    return nullptr;
  }

  Python::Tuple args(2);
  Python::Object argInstance = m_instance;
  args.set(0, argInstance);
  Python::Object fileArg(fileName);
  args.set(1, fileArg);
  auto res = readerFunction.call(args);

  if (!res.isValid()) {
    qCritical("Error calling the reader");
    return nullptr;
  }

  vtkObjectBase* vtkobject =
    Python::VTK::GetPointerFromObject(res, "vtkImageData");
  if (vtkobject == nullptr) {
    return nullptr;
  }

  vtkSmartPointer<vtkImageData> imageData =
    vtkImageData::SafeDownCast(vtkobject);

  if (imageData->GetNumberOfPoints() <= 1) {
    qCritical() << "The file didn't contain any suitable volumetric data";
    return nullptr;
  }
  return imageData;
}

PythonReaderFactory::PythonReaderFactory(QString description,
                                         QStringList extensions,
                                         Python::Object cls)
  : m_description(description), m_extensions(extensions), m_class(cls)
{
}

QString PythonReaderFactory::getDescription() const
{
  return m_description;
}

QStringList PythonReaderFactory::getExtensions() const
{
  return m_extensions;
}

QString PythonReaderFactory::getFileDialogFilter() const
{
  QStringList filterExt;
  foreach (auto ext, m_extensions) {
    filterExt << QString("*.%1").arg(ext);
  }
  QString filter =
    QString("%1 (%2)").arg(m_description).arg(filterExt.join(QString(" ")));

  return filter;
}

PythonReader PythonReaderFactory::createReader() const
{
  Python python;
  auto module = python.import("tomviz.io._internal");
  if (!module.isValid()) {
    qCritical() << "Failed to import tomviz.io._internal module.";
    return Python::Object();
  }
  auto factory = module.findFunction("create_reader_instance");
  if (!factory.isValid()) {
    qCritical()
      << "Failed to import tomviz.io._internal.create_reader_instance";
    return Python::Object();
  }

  Python::Tuple args(1);
  Python::Object argClass = m_class;
  args.set(0, argClass);
  auto res = factory.call(args);
  if (!res.isValid()) {
    qCritical("Error calling create_reader_instance.");
    return Python::Object();
  }
  return PythonReader(res);
}
}

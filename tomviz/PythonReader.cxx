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

PythonReader::PythonReader(QString description, QStringList extensions, Python::Function readFunction)
  : m_description(description), m_extensions(extensions), m_readFunction(readFunction)
{}

QString PythonReader::getDescription() const
{
  return m_description;
}

QStringList PythonReader::getExtensions() const
{
  return m_extensions;
}

DataSource* PythonReader::read(QString fileName)
{
  Python::Tuple args(1);
  args.set(0, Python::Object(fileName));
  auto res = m_readFunction.call(args);
  if (!res.isValid()) {
    qCritical("Error calling the reader");
    return nullptr;
  }
  vtkObjectBase* vtkobject =
      Python::VTK::GetPointerFromObject(res, "vtkImageData");
  vtkImageData* imageData = vtkImageData::SafeDownCast(vtkobject);
  return new DataSource(imageData);
}

}

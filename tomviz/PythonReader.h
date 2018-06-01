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
#ifndef tomvizPythonReader_h
#define tomvizPythonReader_h

#include "PythonUtilities.h"

#include <vtkSmartPointer.h>

#include <QString>
#include <QStringList>

class vtkImageData;

namespace tomviz {
class PythonReader
{
public:
  PythonReader(Python::Object);
  vtkSmartPointer<vtkImageData> read(QString);

private:
  Python::Object m_instance;
};

class PythonReaderFactory
{
public:
  PythonReaderFactory(QString, QStringList, Python::Object);
  QString getDescription() const;
  QStringList getExtensions() const;
  QString getFileDialogFilter() const;
  PythonReader createReader() const;

private:
  QString m_description;
  QStringList m_extensions;
  Python::Object m_class;
};
}

#endif

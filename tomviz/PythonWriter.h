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
#ifndef tomvizPythonWriter_h
#define tomvizPythonWriter_h

#include "PythonUtilities.h"

#include <QString>
#include <QStringList>

namespace tomviz {

class DataSource;

class PythonWriter
{
public:
  PythonWriter(Python::Object);
  bool write(QString fileName, DataSource* data);

private:
  Python::Object m_instance;
};

class PythonWriterFactory
{
public:
  PythonWriterFactory(QString, QStringList, Python::Object);
  QString getDescription() const;
  QStringList getExtensions() const;
  QString getFileDialogFilter() const;
  PythonWriter createWriter() const;

private:
  QString m_description;
  QStringList m_extensions;
  Python::Object m_class;
};
}

#endif

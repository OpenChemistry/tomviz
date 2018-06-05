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
#ifndef tomvizFileFormatManager_h
#define tomvizFileFormatManager_h

#include <QList>
#include <QMap>
#include <QString>

namespace tomviz {

class PythonReaderFactory;
class PythonWriterFactory;

class FileFormatManager
{

public:
  static FileFormatManager& instance();

  // Fetch the available python readers
  void registerPythonReaders();

  // Fetch the available python writer
  void registerPythonWriters();

  QList<PythonReaderFactory*> pythonReaderFactories();
  QList<PythonWriterFactory*> pythonWriterFactories();

  PythonReaderFactory* pythonReaderFactory(const QString& ext);
  PythonWriterFactory* pythonWriterFactory(const QString& ext);

private:
  QMap<QString, PythonReaderFactory*> m_pythonExtReaderMap;
  QMap<QString, PythonWriterFactory*> m_pythonExtWriterMap;
};
} // namespace tomviz

#endif

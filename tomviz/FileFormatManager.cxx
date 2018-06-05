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
#include "FileFormatManager.h"
#include "PythonReader.h"
#include "PythonUtilities.h"
#include "PythonWriter.h"
#include <QDebug>

namespace tomviz {

FileFormatManager& FileFormatManager::instance()
{
  static FileFormatManager theInstance;
  return theInstance;
}

template <typename T>
void registerFactories(const QString& registerFunction,
                       QMap<QString, T*>& factories)
{
  if (!factories.isEmpty()) {
    return;
  }
  Python python;
  auto module = python.import("tomviz.io._internal");

  if (!module.isValid()) {
    qCritical() << "Failed to import tomviz.io._internal module.";
    return;
  }

  auto lister = module.findFunction(registerFunction);
  if (!module.isValid()) {
    qCritical()
      << QString("Failed to find tomviz.io._internal.%1").arg(registerFunction);
    return;
  }

  auto res = lister.call();
  if (!res.isValid()) {
    qCritical() << QString("Error calling %1.").arg(registerFunction);
    return;
  }

  if (res.isList()) {
    Python::List formatList(res);
    for (int i = 0; i < formatList.length(); ++i) {
      Python::List formatInfo(formatList[i]);
      if (!formatInfo.isList() || formatInfo.length() != 3) {
        return;
      }
      QString description = formatInfo[0].toString();
      Python::List extensions_(formatInfo[1]);
      if (!extensions_.isList()) {
        return;
      }
      Python::Object readerClass = formatInfo[2];
      QStringList extensions;
      for (int j = 0; j < extensions_.length(); ++j) {
        extensions << QString(extensions_[j].toString());
      }
      T* factory = new T(description, extensions, readerClass);
      foreach (auto extension, extensions) {
        factories[extension] = factory;
      }
    }
  }
}

void FileFormatManager::registerPythonReaders()
{
  registerFactories<PythonReaderFactory>("list_python_readers",
                                         m_pythonExtReaderMap);
}

void FileFormatManager::registerPythonWriters()
{
  registerFactories<PythonWriterFactory>("list_python_writers",
                                         m_pythonExtWriterMap);
}

QList<PythonReaderFactory*> FileFormatManager::pythonReaderFactories()
{
  return m_pythonExtReaderMap.values().toSet().values();
}

PythonReaderFactory* FileFormatManager::pythonReaderFactory(const QString& ext)
{
  return m_pythonExtReaderMap[ext];
}

QList<PythonWriterFactory*> FileFormatManager::pythonWriterFactories()
{
  return m_pythonExtWriterMap.values().toSet().values();
}

PythonWriterFactory* FileFormatManager::pythonWriterFactory(const QString& ext)
{
  return m_pythonExtWriterMap[ext];
}

} // namespace tomviz

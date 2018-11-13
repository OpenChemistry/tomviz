/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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

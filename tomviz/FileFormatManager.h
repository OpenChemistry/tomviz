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

  // Fetch the list of previously available python readers,
  // and create placeholders for them while the actual python readers are loaded
  void prepopulatePythonReaders();

  // Fetch the list of previously available python writers,
  // and create placeholders for them while the actual python writers are loaded
  void prepopulatePythonWriters();

  // Fetch the available python readers
  void registerPythonReaders();

  // Fetch the available python writer
  void registerPythonWriters();

  QList<PythonReaderFactory*> pythonReaderFactories();
  QList<PythonWriterFactory*> pythonWriterFactories();

  PythonReaderFactory* pythonReaderFactory(const QString& ext);
  PythonWriterFactory* pythonWriterFactory(const QString& ext);

private:
  void setPythonReadersMap(QMap<QString, PythonReaderFactory*> factories);
  void setPythonWritersMap(QMap<QString, PythonWriterFactory*> factories);
  QMap<QString, PythonReaderFactory*> m_pythonExtReaderMap;
  QMap<QString, PythonWriterFactory*> m_pythonExtWriterMap;
};
} // namespace tomviz

#endif

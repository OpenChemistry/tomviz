/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "FileFormatManager.h"
#include "PythonReader.h"
#include "PythonUtilities.h"
#include "PythonWriter.h"
#include <QDebug>

#include <pqApplicationCore.h>
#include <pqSettings.h>

namespace tomviz {

FileFormatManager& FileFormatManager::instance()
{
  static FileFormatManager theInstance;
  return theInstance;
}

template <typename T>
QMap<QString, T*> registerFactories(const QString& name,
                                    const QString& registerFunction)
{
  QMap<QString, T*> factories;

  Python python;
  auto module = python.import("tomviz.io._internal");

  if (!module.isValid()) {
    qCritical() << "Failed to import tomviz.io._internal module.";
    return factories;
  }

  auto lister = module.findFunction(registerFunction);
  if (!module.isValid()) {
    qCritical()
      << QString("Failed to find tomviz.io._internal.%1").arg(registerFunction);
    return factories;
  }

  auto res = lister.call();
  if (!res.isValid()) {
    qCritical() << QString("Error calling %1.").arg(registerFunction);
    return factories;
  }

  if (res.isList()) {
    Python::List formatList(res);
    QList<QPair<QString, QStringList>> cached;
    for (int i = 0; i < formatList.length(); ++i) {
      Python::List formatInfo(formatList[i]);
      if (!formatInfo.isList() || formatInfo.length() != 3) {
        continue;
      }
      QString description = formatInfo[0].toString();
      Python::List extensions_(formatInfo[1]);
      if (!extensions_.isList()) {
        continue;
      }
      Python::Object readerClass = formatInfo[2];
      QStringList extensions;
      for (int j = 0; j < extensions_.length(); ++j) {
        extensions << QString(extensions_[j].toString());
      }
      cached << QPair<QString, QStringList>(description, extensions);
      T* factory = new T(description, extensions, readerClass);
      foreach (auto extension, extensions) {
        factories[extension] = factory;
      }
    }

    auto settings = pqApplicationCore::instance()->settings();
    settings->beginWriteArray(name);
    for (int i = 0; i < cached.size(); ++i) {
      settings->setArrayIndex(i);
      settings->setValue("description", cached.at(i).first);
      settings->setValue("extensions", cached.at(i).second.join(","));
    }
    settings->endArray();
  }

  return factories;
}

template <typename T>
QMap<QString, T*> populateFactories(const QString& name)
{
  QMap<QString, T*> factories;
  auto settings = pqApplicationCore::instance()->settings();
  int size = settings->beginReadArray(name);
  for (int i = 0; i < size; ++i) {
    settings->setArrayIndex(i);
    auto description = settings->value("description").toString();
    auto extensions = settings->value("extensions").toString().split(",");
    T* factory = new T(description, extensions);
    foreach (auto extension, extensions) {
      factories[extension] = factory;
    }
  }
  settings->endArray();
  return factories;
}

void FileFormatManager::prepopulatePythonReaders()
{
  auto factories = populateFactories<PythonReaderFactory>("pythonReaders");
  setPythonReadersMap(factories);
}

void FileFormatManager::prepopulatePythonWriters()
{
  auto factories = populateFactories<PythonWriterFactory>("pythonWriters");
  setPythonWritersMap(factories);
}

void FileFormatManager::registerPythonReaders()
{
  auto factories = registerFactories<PythonReaderFactory>(
    "pythonReaders", "list_python_readers");
  setPythonReadersMap(factories);
}

void FileFormatManager::registerPythonWriters()
{
  auto factories = registerFactories<PythonWriterFactory>(
    "pythonWriters", "list_python_writers");
  setPythonWritersMap(factories);
}

void FileFormatManager::setPythonReadersMap(
  QMap<QString, PythonReaderFactory*> factories)
{
  m_pythonExtReaderMap = factories;
}

void FileFormatManager::setPythonWritersMap(
  QMap<QString, PythonWriterFactory*> factories)
{
  m_pythonExtWriterMap = factories;
}

QList<PythonReaderFactory*> FileFormatManager::pythonReaderFactories()
{
  return m_pythonExtReaderMap.values().toSet().values();
}

PythonReaderFactory* FileFormatManager::pythonReaderFactory(const QString& ext)
{
  return m_pythonExtReaderMap.value(ext, nullptr);
}

QList<PythonWriterFactory*> FileFormatManager::pythonWriterFactories()
{
  return m_pythonExtWriterMap.values().toSet().values();
}

PythonWriterFactory* FileFormatManager::pythonWriterFactory(const QString& ext)
{
  return m_pythonExtWriterMap.value(ext, nullptr);
}

} // namespace tomviz

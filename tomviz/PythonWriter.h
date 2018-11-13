/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPythonWriter_h
#define tomvizPythonWriter_h

#include "PythonUtilities.h"

#include <QString>
#include <QStringList>

class vtkImageData;

namespace tomviz {

class DataSource;

class PythonWriter
{
public:
  PythonWriter(Python::Object);
  bool write(QString fileName, vtkImageData* data);

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

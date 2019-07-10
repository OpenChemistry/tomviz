/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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
  PythonReader();
  vtkSmartPointer<vtkImageData> read(QString);

private:
  Python::Object m_instance;
};

class PythonReaderFactory
{
public:
  PythonReaderFactory(QString, QStringList, Python::Object);
  PythonReaderFactory(QString, QStringList);
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

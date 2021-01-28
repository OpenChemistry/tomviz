/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataBroker_h
#define tomvizDataBroker_h

#include "PythonUtilities.h"

#include <vtkSmartPointer.h>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

class vtkImageData;

Q_DECLARE_METATYPE(vtkSmartPointer<vtkImageData>)

namespace tomviz {

class DataBrokerCall : public QObject
{
  Q_OBJECT

public:
  explicit DataBrokerCall(QObject* parent = 0) : QObject(parent) {}

signals:
  void error(const QString& errorMessage);
};

class ListResourceCall : public DataBrokerCall
{
  Q_OBJECT

public:
  explicit ListResourceCall(QObject* parent = 0) : DataBrokerCall(parent) {}

signals:
  void complete(QList<QVariantMap> results);
};

class LoadDataCall : public DataBrokerCall
{
  Q_OBJECT

public:
  explicit LoadDataCall(QObject* parent = 0) : DataBrokerCall(parent) {}

signals:
  void complete(vtkSmartPointer<vtkImageData> imageData);
};

class DataBroker : public QObject
{
  Q_OBJECT

private:
  Python::Module m_dataBrokerModule;

public:
  DataBroker(QObject* parent = 0);
  bool installed();
  ListResourceCall* catalogs();
  ListResourceCall* runs(const QString& catalog);
  ListResourceCall* tables(const QString& catalog, const QString& runUid);
  ListResourceCall* variables(const QString& catalog, const QString& runUid,
                              const QString& table);
  LoadDataCall* loadVariable(const QString& catalog, const QString& runUid,
                             const QString& table, const QString& variable);
};

} // namespace tomviz

#endif

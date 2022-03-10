/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataBroker.h"

#include "DataSource.h"
#include "Utilities.h"

#include <vtkImageData.h>

#include <QDebug>
#include <QtConcurrent>

namespace tomviz {

DataBroker::DataBroker(QObject* parent) : QObject(parent)
{
  qRegisterMetaType<vtkSmartPointer<vtkImageData>>();
  qRegisterMetaType<QList<QVariantMap>>();

  Python python;
  m_dataBrokerModule = python.import("tomviz.io._databroker");
  if (!m_dataBrokerModule.isValid()) {
    qCritical() << "Failed to import tomviz.io._databroker module.";
  }
}

bool DataBroker::installed()
{
  Python python;

  auto installed = m_dataBrokerModule.findFunction("installed");
  if (!installed.isValid()) {
    qCritical() << "Failed to import tomviz.io._databroker.installed";
    return false;
  }

  auto res = installed.call();

  if (!res.isValid()) {
    qCritical("Error calling installed");
    return false;
  }

  return res.toBool();
}

ListResourceCall* DataBroker::catalogs()
{
  auto call = new ListResourceCall(this);

  auto future = QtConcurrent::run([this, call]() {
    Python python;

    auto catalogsFunc = m_dataBrokerModule.findFunction("catalogs");
    if (!catalogsFunc.isValid()) {
      emit call->error("Failed to import tomviz.io._databroker.catalogs");
      return;
    }
    auto res = catalogsFunc.call();

    if (!res.isValid()) {
      emit call->error("Error fetching catalogs");
      return;
    }

    QList<QVariantMap> catalogs;

    for (auto v : toQVariant(res.toVariant()).toList()) {
      catalogs.append(v.toMap());
    }

    emit call->complete(catalogs);
  });

  return call;
}

ListResourceCall* DataBroker::runs(const QString& catalog, const QString& since,
                                   const QString& until, int limit)
{
  auto call = new ListResourceCall(this);

  auto future = QtConcurrent::run([this, call, catalog, since, until, limit]() {
    Python python;

    auto runsFunc = m_dataBrokerModule.findFunction("runs");
    if (!runsFunc.isValid()) {
      emit call->error("Failed to import tomviz.io._databroker.runs");
      return;
    }

    Python::Tuple args(4);
    args.set(0, catalog.toStdString());
    args.set(1, since.toStdString());
    args.set(2, until.toStdString());
    args.set(3, limit);

    auto res = runsFunc.call(args);

    if (!res.isValid()) {
      emit call->error("Error fetching runs");
      return;
    }

    QList<QVariantMap> runs;

    for (auto v : toQVariant(res.toVariant()).toList()) {
      runs.append(v.toMap());
    }

    emit call->complete(runs);
  });

  return call;
}

ListResourceCall* DataBroker::tables(const QString& catalog,
                                     const QString& runUid)
{
  auto call = new ListResourceCall(this);

  auto future = QtConcurrent::run([this, call, catalog, runUid]() {
    Python python;

    auto tablesFunc = m_dataBrokerModule.findFunction("tables");
    if (!tablesFunc.isValid()) {
      emit call->error("Failed to import tomviz.io._databroker.tables");
      return;
    }

    Python::Tuple args(2);
    args.set(0, catalog.toStdString());
    args.set(1, runUid.toStdString());

    auto res = tablesFunc.call(args);

    if (!res.isValid()) {
      emit call->error("Error fetching tables");
      return;
    }

    QList<QVariantMap> tables;

    for (auto v : toQVariant(res.toVariant()).toList()) {
      tables.append(v.toMap());
    }

    emit call->complete(tables);
  });

  return call;
}

ListResourceCall* DataBroker::variables(const QString& catalog,
                                        const QString& runUid,
                                        const QString& table)
{
  auto call = new ListResourceCall(this);

  auto future = QtConcurrent::run([this, call, catalog, runUid, table]() {
    Python python;

    auto variablesFunc = m_dataBrokerModule.findFunction("variables");
    if (!variablesFunc.isValid()) {
      emit call->error("Failed to import tomviz.io._databroker.variables");
      return;
    }

    Python::Tuple args(3);
    args.set(0, catalog.toStdString());
    args.set(1, runUid.toStdString());
    args.set(2, table.toStdString());

    auto res = variablesFunc.call(args);
    if (!res.isValid()) {
      emit call->error("Error fetching variables");
      return;
    }

    QList<QVariantMap> variables;
    for (auto v : toQVariant(res.toVariant()).toList()) {
      variables.append(v.toMap());
    }

    emit call->complete(variables);
  });

  return call;
}

LoadDataCall* DataBroker::loadVariable(const QString& catalog,
                                       const QString& runUid,
                                       const QString& table,
                                       const QString& variable)
{
  auto call = new LoadDataCall(this);

  auto future = QtConcurrent::run([this, catalog, runUid, table, variable,
                                   call]() {
    Python python;

    auto loadFunc = m_dataBrokerModule.findFunction("load_variable");
    if (!loadFunc.isValid()) {
      emit call->error("Failed to import tomviz.io._databroker.load_variable");
    }

    Python::Tuple args(4);
    args.set(0, catalog.toStdString());
    args.set(1, runUid.toStdString());
    args.set(2, table.toStdString());
    args.set(3, variable.toStdString());

    auto res = loadFunc.call(args);

    if (!res.isValid()) {
      emit call->error("Error calling load_variable");
      return;
    }

    vtkObjectBase* vtkobject =
      Python::VTK::GetPointerFromObject(res, "vtkImageData");
    if (vtkobject == nullptr) {
      emit call->error("Error converting to vtkImageData");
      return;
    }

    vtkSmartPointer<vtkImageData> imageData =
      vtkImageData::SafeDownCast(vtkobject);

    if (imageData->GetNumberOfPoints() <= 1) {
      emit call->error("The file didn't contain any suitable data");
    }

    emit call->complete(imageData);
  });

  return call;
}

} // namespace tomviz

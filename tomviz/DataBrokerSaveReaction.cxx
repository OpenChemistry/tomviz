/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "DataBrokerSaveReaction.h"
#include "DataBrokerSaveDialog.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "GenericHDF5Format.h"
#include "LoadDataReaction.h"
#include "Utilities.h"

#include <vtkImageData.h>

#include <QDebug>
#include <QMessageBox>

namespace tomviz {

DataBrokerSaveReaction::DataBrokerSaveReaction(QAction* parentObject,
                                               MainWindow* mainWindow)
  : pqReaction(parentObject), m_mainWindow(mainWindow)
{
  QObject::connect(
    &ActiveObjects::instance(),
    QOverload<DataSource*>::of(&ActiveObjects::dataSourceChanged), this,
    [this](DataSource* dataSource) {
      parentAction()->setEnabled(dataSource != nullptr &&
                                 m_dataBrokerInstalled);
    });
}

DataBrokerSaveReaction::~DataBrokerSaveReaction() = default;

void DataBrokerSaveReaction::onTriggered()
{
  saveData();
}

void DataBrokerSaveReaction::setDataBrokerInstalled(bool installed)
{
  m_dataBrokerInstalled = installed;
}

void DataBrokerSaveReaction::saveData()
{
  auto dataBroker = new DataBroker(tomviz::mainWidget());
  DataBrokerSaveDialog dialog(dataBroker, tomviz::mainWidget());

  if (dialog.exec() == QDialog::Accepted) {
    auto name = dialog.name();

    auto ds = ActiveObjects::instance().activeDataSource();
    if (ds == nullptr) {
      qWarning() << "No active data source!";
      return;
    }

    auto data = ds->imageData();

    vtkNew<vtkImageData> permutedData;
    if (DataSource::hasTiltAngles(data)) {
      // No deep copies of data needed. Just re-label the axes.
      permutedData->ShallowCopy(data);
      relabelXAndZAxes(permutedData);
    } else {
      // Need to re-order to C ordering before writing
      GenericHDF5Format::reorderData(data, permutedData,
                                     ReorderMode::FortranToC);
    }

    tomviz::mainWidget()->setCursor(Qt::WaitCursor);
    auto call = dataBroker->saveData("fxi", name, data);
    connect(
      call, &SaveDataCall::complete, dataBroker,
      [dataBroker, this](const QString& id) {
        dataBroker->deleteLater();
        tomviz::mainWidget()->unsetCursor();
        QMessageBox messageBox(
          QMessageBox::Information, "tomviz",
          QString(
            "The active dataset was successfully exported to DataBroker: %1")
            .arg(id),
          QMessageBox::Ok, m_mainWindow);
        messageBox.exec();
      });

    connect(call, &DataBrokerCall::error, dataBroker,
            [dataBroker, this](const QString& errorMessage) {
              tomviz::mainWidget()->unsetCursor();
              dataBroker->deleteLater();
              QMessageBox messageBox(
                QMessageBox::Warning, "tomviz",
                QString("Error export data to DataBroker: %1. Please check "
                        "message log for details.")
                  .arg(errorMessage),
                QMessageBox::Ok, m_mainWindow);
              messageBox.exec();
            });
  }
}

} // namespace tomviz

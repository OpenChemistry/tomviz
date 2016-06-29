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
#include "SaveDataReaction.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"
#include "pqSaveDataReaction.h"
#include "pqPipelineSource.h"
#include "pqProxyWidgetDialog.h"
#include "pqFileDialog.h"
#include "vtkDataObject.h"
#include "vtkDataArray.h"
#include "vtkImageData.h"
#include "vtkPointData.h"
#include "vtkSMCoreUtilities.h"
#include "vtkSMParaViewPipelineController.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxyManager.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMWriterFactory.h"
#include "vtkTrivialProducer.h"

#include <cassert>

#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>

namespace tomviz
{

SaveDataReaction::SaveDataReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(updateEnableState()));
  updateEnableState();
}

SaveDataReaction::~SaveDataReaction()
{
}

void SaveDataReaction::updateEnableState()
{
  parentAction()->setEnabled(
        ActiveObjects::instance().activeDataSource() != nullptr);
}

void SaveDataReaction::onTriggered()
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  DataSource *source = ActiveObjects::instance().activeDataSource();
  assert(source);
  vtkSMWriterFactory* writerFactory =
      vtkSMProxyManager::GetProxyManager()->GetWriterFactory();
  QString filters = writerFactory->GetSupportedFileTypes(source->producer());
  if (filters.isEmpty())
  {
    qCritical("Cannot determine writer to use.");
    return;
  }
  // Remove options to output in JPEG or PNG format since they don't support volumes
  QRegularExpression re(";;(JPEG|PNG)[^;]*;;");
  QString filteredFilters = filters.replace(re, ";;");

  pqFileDialog fileDialog(server,
                          pqCoreUtilities::mainWidget(),
                          tr("Save File:"), QString(), filteredFilters);
  fileDialog.setObjectName("FileSaveDialog");
  fileDialog.setFileMode(pqFileDialog::AnyFile);
  // Default to saving tiff files
  fileDialog.setRecentlyUsedExtension(".tiff");
  if (fileDialog.exec() == QDialog::Accepted)
  {
    this->saveData(fileDialog.getSelectedFiles()[0]);
  }
}

bool SaveDataReaction::saveData(const QString &filename)
{
  pqServer* server = pqActiveObjects::instance().activeServer();
  DataSource *source = ActiveObjects::instance().activeDataSource();
  if (!server || !source)
  {
    qCritical("No active source located.");
    return false;
  }

  vtkSMWriterFactory* writerFactory = vtkSMProxyManager::GetProxyManager()->GetWriterFactory();
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(writerFactory->CreateWriter(filename.toLatin1().data(),
                      source->producer()));
  vtkSMSourceProxy* writer = vtkSMSourceProxy::SafeDownCast(proxy);
  if (!writer)
  {
    qCritical() << "Failed to create writer for: " << filename;
    return false;
  }
  if (strcmp(writer->GetClientSideObject()->GetClassName(),"vtkTIFFWriter") == 0)
  {
    vtkTrivialProducer *t =
        vtkTrivialProducer::SafeDownCast(source->producer()->GetClientSideObject());
    vtkImageData* imageData = vtkImageData::SafeDownCast(t->GetOutputDataObject(0));
    if (imageData->GetPointData()->GetScalars()->GetDataType() == VTK_DOUBLE)
    {
      QMessageBox messageBox;
      messageBox.setWindowTitle("Unsupported data type");
      messageBox.setText("Tiff files do not support writing data of type double.");
      messageBox.setIcon(QMessageBox::Critical);
      messageBox.exec();
      return false;
    }
  }

  pqProxyWidgetDialog dialog(writer, pqCoreUtilities::mainWidget());
  dialog.setObjectName("WriterSettingsDialog");
  dialog.setEnableSearchBar(true);
  dialog.setWindowTitle(
    QString("Configure Writer (%1)").arg(writer->GetXMLLabel()));

  // Check to see if this writer has any properties that can be configured by
  // the user. If it does, display the dialog.
  if (dialog.hasVisibleWidgets())
  {
    dialog.exec();
    if(dialog.result() == QDialog::Rejected)
    {
      // The user pressed Cancel so don't write
      return false;
    }
  }
  writer->UpdateVTKObjects();
  writer->UpdatePipeline();
  return true;
}

} // end of namespace tomviz

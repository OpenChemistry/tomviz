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

#include "PipelineView.h"

#include "ActiveObjects.h"
#include "CloneDataReaction.h"
#include "EditOperatorDialog.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "OperatorResult.h"
#include "PipelineModel.h"
#include "SaveDataReaction.h"
#include "ToggleDataTypeReaction.h"
#include "Utilities.h"

#include <pqCoreUtilities.h>
#include <pqSpreadSheetView.h>
#include <pqView.h>
#include <vtkNew.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMViewProxy.h>
#include <vtkTable.h>

#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>

namespace tomviz {

PipelineView::PipelineView(QWidget* p) : QTreeView(p)
{
  connect(this, SIGNAL(clicked(QModelIndex)), SLOT(rowActivated(QModelIndex)));
  setIndentation(20);
  setRootIsDecorated(false);
  setItemsExpandable(false);

  QString customStyle = "QTreeView::branch { background-color: white; }";
  setStyleSheet(customStyle);
  setAlternatingRowColors(true);
  setSelectionBehavior(QAbstractItemView::SelectRows);

  // track selection to update ActiveObjects.
  connect(&ModuleManager::instance(), SIGNAL(dataSourceAdded(DataSource*)),
          SLOT(setCurrent(DataSource*)));
  connect(&ModuleManager::instance(), SIGNAL(moduleAdded(Module*)),
          SLOT(setCurrent(Module*)));

  connect(this, SIGNAL(doubleClicked(QModelIndex)),
          SLOT(rowDoubleClicked(QModelIndex)));
}

PipelineView::~PipelineView() = default;

void PipelineView::keyPressEvent(QKeyEvent* e)
{
  if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
    if (currentIndex().isValid()) {
      deleteItem(currentIndex());
    }
  } else {
    QTreeView::keyPressEvent(e);
  }
}

void PipelineView::contextMenuEvent(QContextMenuEvent* e)
{
  auto idx = indexAt(e->pos());
  if (!idx.isValid()) {
    return;
  }

  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  auto dataSource = pipelineModel->dataSource(idx);
  auto result = pipelineModel->result(idx);

  bool childData =
    (dataSource && qobject_cast<Operator*>(dataSource->parent())) ||
    (result && qobject_cast<Operator*>(result->parent()));
  if (childData) {
    return;
  }

  QMenu contextMenu;
  QAction* cloneAction = nullptr;
  QAction* markAsAction = nullptr;
  QAction* saveDataAction = nullptr;
  if (dataSource != nullptr) {
    cloneAction = contextMenu.addAction("Clone");
    new CloneDataReaction(cloneAction);
    saveDataAction = contextMenu.addAction("Save Data");
    new SaveDataReaction(saveDataAction);
    if (dataSource->type() == DataSource::Volume) {
      markAsAction = contextMenu.addAction("Mark as Tilt Series");
    } else {
      markAsAction = contextMenu.addAction("Mark as Volume");
    }
  }
  auto deleteAction = contextMenu.addAction("Delete");
  auto globalPoint = mapToGlobal(e->pos());
  auto selectedItem = contextMenu.exec(globalPoint);
  // Some action was selected, so process it.
  if (selectedItem == deleteAction) {
    deleteItem(idx);
  } else if (markAsAction != nullptr && markAsAction == selectedItem) {
    auto mainWindow = qobject_cast<QMainWindow*>(window());
    ToggleDataTypeReaction::toggleDataType(mainWindow, dataSource);
  }
}

void PipelineView::deleteItem(const QModelIndex& idx)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  Q_ASSERT(pipelineModel);
  auto dataSource = pipelineModel->dataSource(idx);
  auto module = pipelineModel->module(idx);
  auto op = pipelineModel->op(idx);
  if (dataSource) {
    pipelineModel->removeDataSource(dataSource);
  } else if (module) {
    pipelineModel->removeModule(module);
  } else if (op) {
    pipelineModel->removeOp(op);
  }
  ActiveObjects::instance().renderAllViews();
}

void PipelineView::rowActivated(const QModelIndex& idx)
{
  if (idx.isValid() && idx.column() == 2) {
    auto pipelineModel = qobject_cast<PipelineModel*>(this->model());
    if (pipelineModel) {
      if (auto module = pipelineModel->module(idx)) {
        module->setVisibility(!module->visibility());
        if (pqView* view = tomviz::convert<pqView*>(module->view())) {
          view->render();
        }
      } else if (auto op = pipelineModel->op(idx)) {
        pipelineModel->removeOp(op);
        expandAll();
      }
    }
  }
}

void PipelineView::rowDoubleClicked(const QModelIndex& idx)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  Q_ASSERT(pipelineModel);
  if (auto op = pipelineModel->op(idx)) {
    if (op->hasCustomUI()) {
      // Create a non-modal dialog, delete it once it has been closed.
      EditOperatorDialog* dialog = new EditOperatorDialog(
        op, op->dataSource(), false, pqCoreUtilities::mainWidget());
      dialog->setAttribute(Qt::WA_DeleteOnClose, true);
      dialog->show();
    }
  } else if (auto result = pipelineModel->result(idx)) {
    if (vtkTable::SafeDownCast(result->dataObject())) {
      auto view = ActiveObjects::instance().activeView();

      // If the view is not a SpreadSheetView, look for the first one and
      // use it if possible.
      vtkNew<vtkSMProxyIterator> iter;
      iter->SetSessionProxyManager(ActiveObjects::instance().proxyManager());
      iter->SetModeToOneGroup();
      for (iter->Begin("views"); !iter->IsAtEnd(); iter->Next()) {
        auto viewProxy = vtkSMViewProxy::SafeDownCast(iter->GetProxy());
        if (std::string(viewProxy->GetXMLName()) == "SpreadSheetView") {
          view = viewProxy;
          break;
        }
      }

      // If a spreadsheet view wasn't found, controller->ShowInPreferredView()
      // will create one.
      vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
      view = controller->ShowInPreferredView(result->producerProxy(), 0, view);
      ActiveObjects::instance().setActiveView(view);
    }
  }
}

void PipelineView::currentChanged(const QModelIndex& current,
                                  const QModelIndex&)
{
  if (!current.isValid()) {
    return;
  }
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  Q_ASSERT(pipelineModel);

  if (auto dataSource = pipelineModel->dataSource(current)) {
    ActiveObjects::instance().setActiveDataSource(dataSource);
  } else if (auto module = pipelineModel->module(current)) {
    ActiveObjects::instance().setActiveModule(module);
  } else if (auto op = pipelineModel->op(current)) {
    ActiveObjects::instance().setActiveOperator(op);
  }
}

void PipelineView::setCurrent(DataSource* dataSource)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  setCurrentIndex(pipelineModel->dataSourceIndex(dataSource));
}

void PipelineView::setCurrent(Module* module)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  setCurrentIndex(pipelineModel->moduleIndex(module));
}

void PipelineView::setCurrent(Operator*)
{
}
}

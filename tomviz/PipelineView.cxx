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
#include "ToggleDataTypeReaction.h"
#include "Utilities.h"

#include <pqCoreUtilities.h>
#include <pqSpreadSheetView.h>
#include <pqXYChartView.h>
#include <pqView.h>
#include <vtkNew.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
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
  this->setStyleSheet(customStyle);
  setAlternatingRowColors(true);
  setSelectionBehavior(QAbstractItemView::SelectRows);

  // track selection to update ActiveObjects.
  connect(&ActiveObjects::instance(), SIGNAL(dataSourceChanged(DataSource*)),
          SLOT(setCurrent(DataSource*)));
  connect(&ActiveObjects::instance(), SIGNAL(moduleChanged(Module*)),
          SLOT(setCurrent(Module*)));

  connect(this, SIGNAL(doubleClicked(QModelIndex)),
          SLOT(rowDoubleClicked(QModelIndex)));
}

PipelineView::~PipelineView() = default;

void PipelineView::keyPressEvent(QKeyEvent* e)
{
  QTreeView::keyPressEvent(e);
  if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
    deleteItem(currentIndex());
  }
}

void PipelineView::contextMenuEvent(QContextMenuEvent* e)
{
  auto idx = this->indexAt(e->pos());
  if (!idx.isValid()) {
    return;
  }

  auto pipelineModel = qobject_cast<PipelineModel*>(this->model());
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
  if (dataSource != nullptr) {
    cloneAction = contextMenu.addAction("Clone");
    new CloneDataReaction(cloneAction);
    if (dataSource->type() == DataSource::Volume) {
      markAsAction = contextMenu.addAction("Mark as Tilt Series");
    } else {
      markAsAction = contextMenu.addAction("Mark as Volume");
    }
  }
  QAction* deleteAction = contextMenu.addAction("Delete");
  auto globalPoint = mapToGlobal(e->pos());
  QAction* selectedItem = contextMenu.exec(globalPoint);
  // Some action was selected, so process it.
  if (selectedItem == deleteAction) {
    deleteItem(idx);
  } else if (markAsAction != nullptr && markAsAction == selectedItem) {
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(this->window());
    ToggleDataTypeReaction::toggleDataType(mainWindow, dataSource);
  }
}

void PipelineView::deleteItem(const QModelIndex& idx)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(this->model());
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
  if (idx.isValid() && idx.column() == 1) {
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
  auto pipelineModel = qobject_cast<PipelineModel*>(this->model());
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
      if (tomviz::convert<pqSpreadSheetView*>(view) ||
          tomviz::convert<pqXYChartView*>(view)) {
        vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
        controller->Show(result->producerProxy(), 0, view);
        ActiveObjects::instance().renderAllViews();
      }
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
  auto dataSource = pipelineModel->dataSource(current);
  auto module = pipelineModel->module(current);
  if (dataSource) {
    ActiveObjects::instance().setActiveDataSource(dataSource);
  } else if (module) {
    ActiveObjects::instance().setActiveModule(module);
  }
}

void PipelineView::setCurrent(DataSource* dataSource)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  this->setCurrentIndex(pipelineModel->dataSourceIndex(dataSource));
}

void PipelineView::setCurrent(Module* module)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  this->setCurrentIndex(pipelineModel->moduleIndex(module));
}

void PipelineView::setCurrent(Operator*)
{
}
}

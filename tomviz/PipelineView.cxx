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
#include "ExportDataReaction.h"
#include "LoadDataReaction.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "OperatorPython.h"
#include "OperatorResult.h"
#include "PipelineModel.h"
#include "SaveDataReaction.h"
#include "SnapshotOperator.h"
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

#include <QApplication>
#include <QItemDelegate>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QSet>
#include <QTimer>

namespace tomviz {

class OperatorRunningDelegate : public QItemDelegate
{

public:
  OperatorRunningDelegate(QWidget* parent = 0);

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const;

public slots:
  void start();
  void stop();

private:
  QTimer* m_timer;
  PipelineView* m_view;
  mutable qreal m_angle = 0;
};

OperatorRunningDelegate::OperatorRunningDelegate(QWidget* parent)
  : QItemDelegate(parent)
{
  m_view = qobject_cast<PipelineView*>(parent);
  m_timer = new QTimer(this);
  connect(m_timer, SIGNAL(timeout()), m_view->viewport(), SLOT(update()));
}

void OperatorRunningDelegate::paint(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{

  auto pipelineModel = qobject_cast<PipelineModel*>(m_view->model());
  auto op = pipelineModel->op(index);

  QItemDelegate::paint(painter, option, index);
  if (op && index.column() == Column::state) {
    if (op->state() == OperatorState::Running) {
      QPixmap pixmap(":/icons/spinner.png");

      // Calculate the correct location to draw based on margin. The margin
      // calculation is taken from QItemDelegate::doLayout(...), I couldn't
      // find an API call that would give this to me directly.
      auto leftMargin =
        QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
      QPoint topLeft = option.rect.topLeft();
      topLeft += QPoint(leftMargin, 0);

      int offset = option.rect.height() / 2;
      QRect bounds(-offset, -offset, option.rect.height(),
                   option.rect.height());
      painter->save();
      painter->translate(topLeft.x() + offset, topLeft.y() + offset);
      painter->rotate(m_angle);
      m_angle += 10;
      painter->drawPixmap(bounds, pixmap);
      painter->restore();
    }
  }
}

void OperatorRunningDelegate::start()
{
  m_timer->start(50);
}

void OperatorRunningDelegate::stop()
{
  m_timer->stop();
}

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
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  OperatorRunningDelegate* delegate = new OperatorRunningDelegate(this);
  setItemDelegate(delegate);

  // track selection to update ActiveObjects.
  connect(&ModuleManager::instance(), SIGNAL(dataSourceAdded(DataSource*)),
          SLOT(setCurrent(DataSource*)));
  connect(&ModuleManager::instance(), SIGNAL(moduleAdded(Module*)),
          SLOT(setCurrent(Module*)));

  // Connect up operators to start and stop delegate
  // New datasource added
  connect(&ModuleManager::instance(), &ModuleManager::dataSourceAdded,
          [this, delegate](DataSource* dataSource) {
            // New operator added
            connect(dataSource, &DataSource::operatorAdded, delegate,
                    [this, delegate](Operator* op) {
                      // Connect transformingStarted to OperatorRunningDelegate
                      connect(op, &Operator::transformingStarted, delegate,
                              &OperatorRunningDelegate::start);
                      // Connect transformingDone
                      connect(op, &Operator::transformingDone, delegate,
                              [this, delegate]() { delegate->stop(); });
                    });
          });

  connect(this, SIGNAL(doubleClicked(QModelIndex)),
          SLOT(rowDoubleClicked(QModelIndex)));
}

PipelineView::~PipelineView() = default;

void PipelineView::keyPressEvent(QKeyEvent* e)
{
  if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
    if (enableDeleteItems(selectedIndexes())) {
      deleteItemsConfirm(selectedIndexes());
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

  bool childDataSource =
    (dataSource && ModuleManager::instance().isChild(dataSource));

  if (result && qobject_cast<Operator*>(result->parent())) {
    return;
  }

  QMenu contextMenu;
  QAction* cloneAction = nullptr;
  QAction* markAsAction = nullptr;
  QAction* saveDataAction = nullptr;
  QAction* exportModuleAction = nullptr;
  QAction* executeAction = nullptr;
  QAction* hideAction = nullptr;
  QAction* showAction = nullptr;
  QAction* cloneChildAction = nullptr;
  QAction* snapshotAction = nullptr;
  QAction* showInterfaceAction = nullptr;
  bool allowReExecute = false;

  // Data source ( non child )
  if (dataSource != nullptr && !childDataSource) {
    cloneAction = contextMenu.addAction("Clone");
    new CloneDataReaction(cloneAction);
    saveDataAction = contextMenu.addAction("Save Data");
    new SaveDataReaction(saveDataAction);
    if (dataSource->type() == DataSource::Volume) {
      markAsAction = contextMenu.addAction("Mark as Tilt Series");
    } else {
      markAsAction = contextMenu.addAction("Mark as Volume");
    }

    // Add option to re-execute the pipeline is we have a canceled operator
    // in our pipeline.
    foreach (Operator* op, dataSource->operators()) {
      if (op->isCanceled() || op->isModified()) {
        allowReExecute = true;
        break;
      }
    }
    // Child data source
  } else if (childDataSource) {
    cloneChildAction = contextMenu.addAction("Clone");
    saveDataAction = contextMenu.addAction("Save Data");
    new SaveDataReaction(saveDataAction);
  }

  // Allow pipeline to be re-executed if we are dealing with a canceled
  // operator.
  auto op = pipelineModel->op(idx);
  allowReExecute =
    allowReExecute || (op && (op->isCanceled() || op->isModified()));

  if (allowReExecute) {
    executeAction = contextMenu.addAction("Re-execute pipeline");
  }

  // Offer to cache for operators.
  if (op) {
    snapshotAction = contextMenu.addAction("Snapshot Data");
  }

  // Add a view source entry when it is a Python-based operator.
  if (op && qobject_cast<OperatorPython*>(op)) {
    showInterfaceAction = contextMenu.addAction("View Source");
  } else if (op && op->hasCustomUI()) {
    showInterfaceAction = contextMenu.addAction("Edit");
  }

  // Keep the delete menu entry at the end of the list of options.
  QAction* deleteAction = nullptr;
  deleteAction = contextMenu.addAction("Delete");
  if (deleteAction && !enableDeleteItems(selectedIndexes())) {
    deleteAction->setEnabled(false);
  }

  bool allModules = true;
  foreach (auto i, selectedIndexes()) {
    auto module = pipelineModel->module(i);
    if (!module) {
      allModules = false;
      break;
    }
  }

  if (allModules) {
    hideAction = contextMenu.addAction("Hide");
    showAction = contextMenu.addAction("Show");

    if (selectedIndexes().size() == 2) {
      auto module = pipelineModel->module(selectedIndexes()[0]);
      QString exportType = module->exportDataTypeString();
      if (exportType.size() > 0) {
        QString menuActionString = QString("Export as %1").arg(exportType);
        exportModuleAction = contextMenu.addAction(menuActionString);
        new ExportDataReaction(exportModuleAction, module);
      }
    }
  }

  auto globalPoint = mapToGlobal(e->pos());
  auto selectedItem = contextMenu.exec(globalPoint);

  // Nothing selected
  if (!selectedItem) {
    return;
  }

  // Some action was selected, so process it.
  if (selectedItem == deleteAction) {
    deleteItemsConfirm(selectedIndexes());
  } else if (executeAction && selectedItem == executeAction) {
    if (!dataSource) {
      dataSource = op->dataSource();
    }
    dataSource->executeOperators();
  } else if (markAsAction != nullptr && markAsAction == selectedItem) {
    auto mainWindow = qobject_cast<QMainWindow*>(window());
    ToggleDataTypeReaction::toggleDataType(mainWindow, dataSource);
  } else if (hideAction && selectedItem == hideAction) {
    setModuleVisibility(selectedIndexes(), false);
  } else if (showAction && selectedItem == showAction) {
    setModuleVisibility(selectedIndexes(), true);
  } else if (cloneChildAction && selectedItem == cloneChildAction) {
    DataSource* newClone = dataSource->clone(false, true);
    LoadDataReaction::dataSourceAdded(newClone);
  } else if (snapshotAction && selectedItem == snapshotAction) {
    op->dataSource()->addOperator(new SnapshotOperator(op->dataSource()));
  } else if (showInterfaceAction && selectedItem == showInterfaceAction) {
    showUserInterface(op);
  }
}

void PipelineView::deleteItems(const QModelIndexList& idxs)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  Q_ASSERT(pipelineModel);

  QList<DataSource*> dataSources;
  QList<Operator*> operators;
  QList<Module*> modules;

  foreach (QModelIndex idx, idxs) {
    // Make sure we only process one index per row otherwise we try to delete
    // things twice
    if (idx.column() != 0) {
      continue;
    }
    auto dataSource = pipelineModel->dataSource(idx);
    auto module = pipelineModel->module(idx);
    auto op = pipelineModel->op(idx);
    if (dataSource) {
      dataSources.push_back(dataSource);
    } else if (module) {
      modules.push_back(module);
    } else if (op) {
      operators.push_back(op);
    }
  }

  foreach (Module* module, modules) {
    // If the datasource is being remove don't bother removing the module
    if (!dataSources.contains(module->dataSource())) {
      pipelineModel->removeModule(module);
    }
  }

  QSet<DataSource*> paused;
  foreach (Operator* op, operators) {
    // If the datasource is being remove don't bother removing the operator
    if (!dataSources.contains(op->dataSource())) {
      op->dataSource()->pausePipeline();
      paused.insert(op->dataSource());
      pipelineModel->removeOp(op);
    }
  }

  foreach (DataSource* dataSource, dataSources) {
    pipelineModel->removeDataSource(dataSource);
  }

  // Now resume the pipelines
  foreach (DataSource* dataSource, paused) {
    dataSource->resumePipeline();
  }

  // Delay rendering until signals have been processed and all modules removed.
  QTimer::singleShot(0, []() { ActiveObjects::instance().renderAllViews(); });
}

void PipelineView::rowActivated(const QModelIndex& idx)
{
  if (idx.isValid() && idx.column() == Column::state) {
    auto pipelineModel = qobject_cast<PipelineModel*>(model());
    if (pipelineModel) {
      if (auto module = pipelineModel->module(idx)) {
        module->setVisibility(!module->visibility());
        emit model()->dataChanged(idx, idx);
        if (pqView* view = tomviz::convert<pqView*>(module->view())) {
          view->render();
        }
      }
    }
  }
}

void PipelineView::rowDoubleClicked(const QModelIndex& idx)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  Q_ASSERT(pipelineModel);
  if (auto op = pipelineModel->op(idx)) {
    showUserInterface(op);
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

  // Always change the active OperatorResult. It is possible to have both
  // a DataSource and an OperatorResult active at the same time, but only
  // when the OperatorResult is currently selected. If the OperatorResult is
  // not selected, the current active result should be null.
  if (auto result = pipelineModel->result(current)) {
    ActiveObjects::instance().setActiveOperatorResult(result);
  } else {
    ActiveObjects::instance().setActiveOperatorResult(nullptr);
  }

  // Unset active result
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

void PipelineView::deleteItemsConfirm(const QModelIndexList& idxs)
{
  QMessageBox::StandardButton response = QMessageBox::question(
    this, "Delete pipeline elements?",
    "Are you sure you want to delete the selected pipeline elements");
  if (response == QMessageBox::Yes) {
    deleteItems(idxs);
  }
}

bool PipelineView::enableDeleteItems(const QModelIndexList& idxs)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  for (auto& index : idxs) {
    auto dataSource = pipelineModel->dataSource(index);
    if (dataSource && dataSource->isRunningAnOperator()) {
      return false;
    }
    auto op = pipelineModel->op(index);
    if (op && op->dataSource()->isRunningAnOperator()) {
      return false;
    }
  }
  return true;
}

void PipelineView::setModuleVisibility(const QModelIndexList& idxs,
                                       bool visible)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());

  Module* module = nullptr;
  foreach (auto idx, idxs) {
    module = pipelineModel->module(idx);
    module->setVisibility(visible);
  }

  if (module) {
    if (pqView* view = tomviz::convert<pqView*>(module->view())) {
      view->render();
    }
  }
}

void PipelineView::unmapOperatorDialog(Operator* op)
{
  if (op && m_operatorDialogs.contains(op)) {
    m_operatorDialogs.remove(op);
  }
}

void PipelineView::showUserInterface(Operator* op)
{
  if (!op) {
    return;
  }

  if (op->hasCustomUI()) {
    // See if we already have a dialog open for this operator
    bool haveDialog = m_operatorDialogs.contains(op) && m_operatorDialogs[op];
    if (haveDialog) {
      auto dialog = m_operatorDialogs[op];
      dialog->show();
      dialog->raise();
      dialog->activateWindow();
    } else {
      // Create a non-modal dialog, delete it once it has been closed.
      QString dialogTitle("Edit - ");
      dialogTitle.append(op->label());
      auto dialog = new EditOperatorDialog(op, op->dataSource(), false,
                                           pqCoreUtilities::mainWidget());
      dialog->setAttribute(Qt::WA_DeleteOnClose, true);
      dialog->setWindowTitle(dialogTitle);
      dialog->show();
      m_operatorDialogs[op] = dialog;

      // Close the dialog if the Operator is destroyed.
      connect(op, SIGNAL(destroyed()), dialog, SLOT(reject()));
      connect(op, SIGNAL(aboutToBeDestroyed(Operator*)), this,
              SLOT(unmapOperatorDialog(Operator*)));
    }
  }
}
}

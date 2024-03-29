/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineView.h"

#include "ActiveObjects.h"
#include "CloneDataReaction.h"
#include "ConformVolumeReaction.h"
#include "DuplicateModuleReaction.h"
#include "EditOperatorDialog.h"
#include "ExportDataReaction.h"
#include "LoadDataReaction.h"
#include "MergeImagesReaction.h"
#include "Module.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "OperatorPython.h"
#include "OperatorResult.h"
#include "Pipeline.h"
#include "PipelineModel.h"
#include "SaveDataReaction.h"
#include "SetDataTypeReaction.h"
#include "SnapshotOperator.h"
#include "Utilities.h"

#include <pqSpreadSheetView.h>
#include <pqView.h>
#include <vtkNew.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMProxyIterator.h>
#include <vtkSMViewProxy.h>
#include <vtkTable.h>

#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QItemDelegate>
#include <QItemSelection>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
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

  // Connect up operators to start and stop delegate
  // New datasource added
  connect(&ModuleManager::instance(), &ModuleManager::dataSourceAdded,
          [delegate](DataSource* dataSource) {
            // New operator added
            connect(dataSource, &DataSource::operatorAdded, delegate,
                    [delegate](Operator* op) {
                      // Connect transformingStarted to OperatorRunningDelegate
                      connect(op, &Operator::transformingStarted, delegate,
                              &OperatorRunningDelegate::start);
                      // Connect transformingDone
                      connect(op, &Operator::transformingDone, delegate,
                              [delegate]() { delegate->stop(); });
                    });
          });

  connect(&ModuleManager::instance(), &ModuleManager::pipelineViewRenderNeeded,
          viewport(), QOverload<>::of(&QWidget::update));

  connect(this, SIGNAL(doubleClicked(QModelIndex)),
          SLOT(rowDoubleClicked(QModelIndex)));
}

void PipelineView::setModel(QAbstractItemModel* model)
{
  QTreeView::setModel(model);
  auto pipelineModel = qobject_cast<PipelineModel*>(model);
  if (!pipelineModel) {
    // Warn about impending segfault
    qCritical() << "Unknown model type.  PipelineView will not work correctly";
  }

  // Listen for new items being added since the current selection needs
  // to be updated.  We can't listen on the ModuleManager/DataSource
  // since the PipelineModel listens to those signals and setCurrent
  // has to happen AFTER the slots on the PipelineModel are called.  So
  // we listen to the model and respond after it does its update.
  connect(pipelineModel, SIGNAL(dataSourceItemAdded(DataSource*)),
          SLOT(setCurrent(DataSource*)));
  connect(pipelineModel, SIGNAL(childDataSourceItemAdded(DataSource*)),
          SLOT(setCurrent(DataSource*)));
  connect(pipelineModel, SIGNAL(moleculeSourceItemAdded(MoleculeSource*)),
          SLOT(setCurrent(MoleculeSource*)));
  connect(pipelineModel, SIGNAL(moleculeSourceItemAdded(MoleculeSource*)),
          SLOT(setCurrent(MoleculeSource*)));
  connect(pipelineModel, SIGNAL(moduleItemAdded(Module*)),
          SLOT(setCurrent(Module*)));
  connect(pipelineModel, SIGNAL(operatorItemAdded(Operator*)),
          SLOT(setCurrent(Operator*)));
  connect(pipelineModel, SIGNAL(dataSourceModified(DataSource*)),
          SLOT(setCurrent(DataSource*)));

  // This is needed to work around a bug in Qt 5.10, the select resize mode is
  // setting reset for some reason.
  connect(pipelineModel, &PipelineModel::operatorItemAdded, this,
          &PipelineView::initLayout);
}

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

  QMenu contextMenu;
  QAction* cloneAction = nullptr;
  QAction* duplicateModuleAction = nullptr;
  QAction* markAsVolumeAction = nullptr;
  QAction* markAsTiltAction = nullptr;
  QAction* markAsFibAction = nullptr;
  QAction* saveDataAction = nullptr;
  QAction* exportModuleAction = nullptr;
  QAction* executeAction = nullptr;
  QAction* hideAction = nullptr;
  QAction* showAction = nullptr;
  QAction* cloneChildAction = nullptr;
  QAction* snapshotAction = nullptr;
  QAction* showInterfaceAction = nullptr;
  QAction* exportTableResultAction = nullptr;
  QAction* reloadAndResampleAction = nullptr;
  bool allowReExecute = false;
  CloneDataReaction* cloneReaction;

  if (result && qobject_cast<Operator*>(result->parent())) {
    if (vtkTable::SafeDownCast(result->dataObject())) {
      exportTableResultAction = contextMenu.addAction("Save as JSON");
    } else {
      return;
    }
  } else if (dataSource != nullptr) {
    if (!childDataSource) {
      // Data source ( non child )
      cloneAction = contextMenu.addAction("Clone");
      cloneReaction = new CloneDataReaction(cloneAction);
      if (dataSource->type() == DataSource::Volume) {
        markAsTiltAction = contextMenu.addAction("Mark as Tilt Series");
        // markAsFibAction = contextMenu.addAction("Mark as Focused Ion Beam");
      } else if (dataSource->type() == DataSource::TiltSeries) {
        markAsVolumeAction = contextMenu.addAction("Mark as Volume");
        // markAsFibAction = contextMenu.addAction("Mark as Focused Ion Beam");
      } else if (dataSource->type() == DataSource::FIB) {
        markAsVolumeAction = contextMenu.addAction("Mark as Volume");
        markAsTiltAction = contextMenu.addAction("Mark as Tilt Series");
      }

      if (dataSource->canReloadAndResample()) {
        reloadAndResampleAction = contextMenu.addAction("Reload and Resample");
      }

      // Add option to re-execute the pipeline is we have a canceled operator
      // in our pipeline.
      foreach (Operator* op, dataSource->operators()) {
        if (op->isCanceled() || op->isModified()) {
          allowReExecute = true;
          break;
        }
      }
    } else if (childDataSource) {
      // Child data source
      cloneChildAction = contextMenu.addAction("Clone");
    }

    saveDataAction = contextMenu.addAction("Save Data");
    new SaveDataReaction(saveDataAction);

    // Add option to merge different datasets
    QAction* mergeImageAction = contextMenu.addAction("Merge Images");
    auto micReaction = new MergeImagesReaction(mergeImageAction);

    auto conformVolumeAction = contextMenu.addAction("Conform Volume");
    auto cvReaction = new ConformVolumeReaction(conformVolumeAction);

    // Set the selected data sources in the merge components reaction
    QModelIndexList indexList = selectedIndexes();
    QSet<DataSource*> selectedDataSources;
    for (int i = 0; i < indexList.size(); ++i) {
      auto source = pipelineModel->dataSource(indexList[i]);
      if (source && !selectedDataSources.contains(source)) {
        selectedDataSources.insert(source);
      }
    }
    micReaction->updateDataSources(selectedDataSources);
    cvReaction->updateDataSources(selectedDataSources);
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

  // Don't add a "Delete" menu entry for "Output" data source.
  QAction* deleteAction = nullptr;
  bool addDelete = true;
  if (dataSource != nullptr) {
    addDelete = dataSource->forkable();
  }

  if (result) {
    addDelete = false;
  }

  if (addDelete) {
    deleteAction = contextMenu.addAction("Delete");
    if (deleteAction && !enableDeleteItems(selectedIndexes())) {
      deleteAction->setEnabled(false);
    }
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

      duplicateModuleAction = contextMenu.addAction("Duplicate Module");
      new DuplicateModuleReaction(duplicateModuleAction);
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
    // Re-execute from the beginning
    dataSource->pipeline()->resume();
    dataSource->pipeline()
      ->execute(dataSource, dataSource->operators().first())
      ->deleteWhenFinished();
  } else if (markAsVolumeAction != nullptr &&
             markAsVolumeAction == selectedItem) {
    auto mainWindow = qobject_cast<QMainWindow*>(window());
    SetDataTypeReaction::setDataType(mainWindow, dataSource,
                                     DataSource::Volume);
  } else if (markAsTiltAction != nullptr && markAsTiltAction == selectedItem) {
    auto mainWindow = qobject_cast<QMainWindow*>(window());
    SetDataTypeReaction::setDataType(mainWindow, dataSource,
                                     DataSource::TiltSeries);
  } else if (markAsFibAction != nullptr && markAsFibAction == selectedItem) {
    auto mainWindow = qobject_cast<QMainWindow*>(window());
    SetDataTypeReaction::setDataType(mainWindow, dataSource, DataSource::FIB);
  } else if (hideAction && selectedItem == hideAction) {
    setModuleVisibility(selectedIndexes(), false);
  } else if (showAction && selectedItem == showAction) {
    setModuleVisibility(selectedIndexes(), true);
  } else if (cloneChildAction && selectedItem == cloneChildAction) {
    cloneReaction->clone(dataSource);
  } else if (snapshotAction && selectedItem == snapshotAction) {
    op->dataSource()->addOperator(new SnapshotOperator(op->dataSource()));
  } else if (showInterfaceAction && selectedItem == showInterfaceAction) {
    if (qobject_cast<OperatorPython*>(op)) {
      EditOperatorDialog::showDialogForOperator(op, QStringLiteral("viewCode"));
    } else {
      EditOperatorDialog::showDialogForOperator(op);
    }
  } else if (selectedItem == exportTableResultAction) {
    exportTableAsJson(vtkTable::SafeDownCast(result->dataObject()));
  } else if (selectedItem == reloadAndResampleAction) {
    dataSource->reloadAndResample();
  }
}

void PipelineView::exportTableAsJson(vtkTable* table)
{
  auto json = tableToJson(table);
  jsonToFile(json);
}

void PipelineView::deleteItems(const QModelIndexList& idxs)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  Q_ASSERT(pipelineModel);

  QList<DataSource*> dataSources;
  QList<MoleculeSource*> moleculeSources;
  QList<Operator*> operators;
  QList<Module*> modules;

  foreach (QModelIndex idx, idxs) {
    // Make sure we only process one index per row otherwise we try to delete
    // things twice
    if (idx.column() != 0) {
      continue;
    }
    auto dataSource = pipelineModel->dataSource(idx);
    auto moleculeSource = pipelineModel->moleculeSource(idx);
    auto module = pipelineModel->module(idx);
    auto op = pipelineModel->op(idx);
    if (dataSource) {
      dataSources.push_back(dataSource);
    } else if (moleculeSource) {
      moleculeSources.push_back(moleculeSource);
    } else if (module) {
      modules.push_back(module);
    } else if (op) {
      operators.push_back(op);
    }
  }

  foreach (Module* module, modules) {
    // If the datasource is being remove don't bother removing the module
    if (!dataSources.contains(module->dataSource()) &&
        !moleculeSources.contains(module->moleculeSource())) {
      pipelineModel->removeModule(module);
    }
  }

  foreach (MoleculeSource* moleculeSource, moleculeSources) {
    pipelineModel->removeMoleculeSource(moleculeSource);
  }

  QSet<DataSource*> paused;
  foreach (Operator* op, operators) {
    // If the datasource is being remove don't bother removing the operator
    if (!dataSources.contains(op->dataSource())) {
      op->dataSource()->pipeline()->pause();
      paused.insert(op->dataSource());
      pipelineModel->removeOp(op);
    }
  }

  foreach (DataSource* dataSource, dataSources) {
    pipelineModel->removeDataSource(dataSource);
  }

  // Now resume the pipelines
  foreach (DataSource* dataSource, paused) {
    dataSource->pipeline()->resume();
    dataSource->pipeline()->execute(dataSource)->deleteWhenFinished();
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
    EditOperatorDialog::showDialogForOperator(op);
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

  // First set the selected data source to nullptr, in case the new selection
  // is not a data source.
  ActiveObjects::instance().setSelectedDataSource(nullptr);
  if (auto dataSource = pipelineModel->dataSource(current)) {
    ActiveObjects::instance().setSelectedDataSource(dataSource);
  } else if (auto module = pipelineModel->module(current)) {
    ActiveObjects::instance().setActiveModule(module);
  } else if (auto op = pipelineModel->op(current)) {
    ActiveObjects::instance().setActiveOperator(op);
  } else if (auto moleculeSource = pipelineModel->moleculeSource(current)) {
    ActiveObjects::instance().setActiveMoleculeSource(moleculeSource);
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
  auto index = pipelineModel->dataSourceIndex(dataSource);
  setCurrentIndex(index);
  selectionModel()->select(index, QItemSelectionModel::Select);
}

void PipelineView::setCurrent(MoleculeSource* dataSource)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  auto index = pipelineModel->moleculeSourceIndex(dataSource);
  setCurrentIndex(index);
  selectionModel()->select(index, QItemSelectionModel::Select);
}

void PipelineView::setCurrent(Module* module)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  auto index = pipelineModel->moduleIndex(module);
  setCurrentIndex(index);
  selectionModel()->select(index, QItemSelectionModel::Select);
}

void PipelineView::setCurrent(Operator* op)
{
  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  auto index = pipelineModel->operatorIndex(op);
  setCurrentIndex(index);
  selectionModel()->select(index, QItemSelectionModel::Select);
}

void PipelineView::deleteItemsConfirm(const QModelIndexList& idxs)
{
  if (idxs.isEmpty()) {
    return;
  }

  QMessageBox::StandardButton response = QMessageBox::question(
    this, "Delete pipeline elements?",
    "Are you sure you want to delete the selected pipeline elements");
  if (response == QMessageBox::Yes) {
    deleteItems(idxs);
  }
}

bool PipelineView::enableDeleteItems(const QModelIndexList& idxs)
{
  if (idxs.isEmpty()) {
    return false;
  }

  auto pipelineModel = qobject_cast<PipelineModel*>(model());
  for (auto& index : idxs) {
    auto dataSource = pipelineModel->dataSource(index);
    if (dataSource != nullptr) {
      // Disable if pipeline is running
      bool disable =
        dataSource->pipeline() && dataSource->pipeline()->isRunning();

      // Disable for "Output" data sources
      disable = disable || !dataSource->forkable();

      return !disable;
    }

    auto op = pipelineModel->op(index);
    if (op && op->dataSource()->pipeline() &&
        (op->dataSource()->pipeline()->isRunning() ||
         op->state() == OperatorState::Edit)) {
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

void PipelineView::initLayout()
{
  header()->setStretchLastSection(false);
  header()->setVisible(false);
  header()->setSectionResizeMode(0, QHeaderView::Stretch);
  header()->setSectionResizeMode(1, QHeaderView::Fixed);
  header()->resizeSection(1, 30);
}
} // namespace tomviz

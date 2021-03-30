/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Pipeline.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "DockerExecutor.h"
#include "DockerUtilities.h"
#include "ExternalPythonExecutor.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "ThreadedExecutor.h"
#include "Utilities.h"

#include <QMetaEnum>

#include <pqApplicationCore.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkSMViewProxy.h>
#include <vtkTrivialProducer.h>

namespace tomviz {

PipelineSettings::PipelineSettings()
{
  m_settings = pqApplicationCore::instance()->settings();
}

void PipelineSettings::setExecutionMode(Pipeline::ExecutionMode executor)
{
  auto metaEnum = QMetaEnum::fromType<Pipeline::ExecutionMode>();
  setExecutionMode(QString(metaEnum.valueToKey(executor)));
}

void PipelineSettings::setExecutionMode(const QString& executor)
{
  m_settings->setValue("pipeline/mode", executor);
}

Pipeline::ExecutionMode PipelineSettings::executionMode()
{
  if (!m_settings->contains("pipeline/mode")) {
    return Pipeline::ExecutionMode::Threaded;
  }
  auto metaEnum = QMetaEnum::fromType<Pipeline::ExecutionMode>();

  return static_cast<Pipeline::ExecutionMode>(metaEnum.keyToValue(
    m_settings->value("pipeline/mode").toString().toLatin1().data()));
}

QString PipelineSettings::dockerImage()
{
  return m_settings->value("pipeline/docker.image").toString();
}

bool PipelineSettings::dockerPull()
{
  return m_settings->value("pipeline/docker.pull", true).toBool();
}

bool PipelineSettings::dockerRemove()
{
  return m_settings->value("pipeline/docker.remove", true).toBool();
}

QString PipelineSettings::externalPythonExecutablePath()
{
  return m_settings->value("pipeline/external.executable").toString();
}

void PipelineSettings::setDockerImage(const QString& image)
{
  m_settings->setValue("pipeline/docker.image", image);
}

void PipelineSettings::setDockerPull(bool pull)
{
  m_settings->setValue("pipeline/docker.pull", pull);
}

void PipelineSettings::setDockerRemove(bool remove)
{
  m_settings->setValue("pipeline/docker.remove", remove);
}

void PipelineSettings::setExternalPythonExecutablePath(
  const QString& executable)
{
  m_settings->setValue("pipeline/external.executable", executable);
}

Pipeline::Pipeline(DataSource* dataSource, QObject* parent) : QObject(parent)
{
  m_data = dataSource;
  m_data->setParent(this);

  addDataSource(dataSource);

  PipelineSettings settings;
  auto executor = settings.executionMode();
  setExecutionMode(executor);
}

Pipeline::~Pipeline() = default;

Pipeline::Future* Pipeline::execute()
{
  return execute(m_data);
}

class PipelineFutureInternal : public Pipeline::Future
{
  Q_OBJECT
public:
  PipelineFutureInternal(Pipeline* pipeline, QList<Operator*> operators,
                         Pipeline::Future* future, bool recurse = true,
                         QObject* parent = nullptr);

private:
  Pipeline* m_pipeline;
  QScopedPointer<Pipeline::Future> m_currentBranchFuture;
  bool m_recurse = false;

  void setCurrentFuture(Pipeline::Future* future);
};

PipelineFutureInternal::PipelineFutureInternal(Pipeline* pipeline,
                                               QList<Operator*> operators,
                                               Pipeline::Future* future,
                                               bool recurse, QObject* parent)
  : Pipeline::Future(operators, parent), m_pipeline(pipeline),
    m_recurse(recurse)
{
  setCurrentFuture(future);
}

void PipelineFutureInternal::setCurrentFuture(Pipeline::Future* future)
{

  m_currentBranchFuture.reset(future);

  connect(future, &Pipeline::Future::finished, future, [future, this]() {
    auto operators = future->operators();
    // operators will be empty if we are returning the original data set ( i.e
    // no operators have been run.
    if (operators.isEmpty()) {
      setResult(future->result());
      emit finished();
      return;
    }

    auto lastOp = future->operators().last();

    // Do we have another branch to execute
    if (m_recurse && lastOp->childDataSource() != nullptr &&
        !lastOp->childDataSource()->operators().isEmpty()) {
      auto child = lastOp->childDataSource();
      auto newFuture = m_pipeline->executor()->execute(child->dataObject(),
                                                       child->operators());
      this->setCurrentFuture(newFuture);
      // Ensure the pipeline has ownership of the transformed data source.
      lastOp->childDataSource()->setParent(m_pipeline);
    }
    // The pipeline execution is finished
    else {
      setResult(future->result());
      emit finished();
    }
  });
}

Pipeline::Future* Pipeline::execute(DataSource* dataSource)
{
  if (beingEdited(dataSource)) {
    return emptyFuture();
  }

  auto operators = dataSource->operators();
  if (operators.size() < 1) {
    emit finished();
    return emptyFuture();
  }

  Operator* firstModifiedOperator = operators.first();
  if (!isModified(dataSource, &firstModifiedOperator)) {
    emit finished();
    return emptyFuture();
  }

  return execute(dataSource, firstModifiedOperator);
}

Pipeline::Future* Pipeline::execute(DataSource* ds, Operator* start)
{
  return execute(ds, start, nullptr);
}

Pipeline::Future* Pipeline::execute(DataSource* ds, Operator* start,
                                    Operator* end)
{
  if (paused()) {
    return emptyFuture();
  }

  if (ds == nullptr) {
    ds = m_data;
  }

  m_operatorsDeleted = false;

  emit started();

  auto operators = ds->operators();
  if (operators.isEmpty()) {
    emit finished();
    auto future = new Pipeline::Future();
    // Delay emitting signal until next event loop
    QTimer::singleShot(0, [future, ds] { emit future->finished(); });
    return future;
  }
  int startIndex = 0;
  // We currently only support running the last operator or the entire pipeline.
  if (start == nullptr) {
    start = operators.first();
  }
  if (start == ds->operators().last() && start->isNew()) {
    // See if we have any canceled operators in the pipeline, if so we have to
    // re-run the anyway pipeline.
    bool haveCanceled = false;
    for (auto itr = operators.begin(); *itr != start; ++itr) {
      auto currentOp = *itr;
      if (currentOp->isCanceled()) {
        currentOp->resetState();
        haveCanceled = true;
        break;
      }
    }

    if (!haveCanceled) {
      startIndex = operators.indexOf(start);
      // Use transformed data source
      ds = transformedDataSource(ds);
    }
  }

  // If we have been asked to run until the new operator we can just return
  // the transformed data.
  if (!operators.isEmpty() && end != nullptr && end->isNew()) {
    auto transformed = transformedDataSource();
    auto dataObject = vtkImageData::SafeDownCast(transformed->copyData());
    auto future = new Pipeline::Future(dataObject);
    dataObject->FastDelete();
    // Delay emitting signal until next event loop
    QTimer::singleShot(0, [=] { emit future->finished(); });

    return future;
  }

  auto endIndex = -1;
  if (end != nullptr) {
    endIndex = operators.indexOf(end);
  }

  auto branchFuture =
    m_executor->execute(ds->dataObject(), operators, startIndex, endIndex);
  connect(branchFuture, &Pipeline::Future::finished, this,
          &Pipeline::branchFinished);

  auto pipelineFuture = new PipelineFutureInternal(
    this, branchFuture->operators(), branchFuture, operators.last() == end);

  // After the pipeline finishes, move the modules to the back and
  // remove the old child data source.
  // If the operator to be removed has a child, move those modules to
  // the back. Otherwise, move the transformed data source modules.
  DataSource* oldChild = nullptr;
  if (m_lastOperatorChildRemoved) {
    // This indicates that an operator was just removed. Use that.
    oldChild = m_lastOperatorChildRemoved;
    m_lastOperatorChildRemoved = nullptr;
  } else {
    // Get either the child data source of the last operator or the
    // transformed data source (which could be the root data source).
    auto lastOp = operators.last();
    oldChild = lastOp->childDataSource() ? lastOp->childDataSource()
                                         : transformedDataSource();
  }

  connect(pipelineFuture, &Pipeline::Future::finished, oldChild,
          [this, oldChild]() {
            auto newChild = transformedDataSource();
            if (newChild != oldChild) {
              // If the child is not the same, move the modules to the new one
              ModuleManager::instance().moveModules(oldChild, newChild);
              if (!oldChild->forkable()) {
                // Remove the old child data source if it was not forkable
                ModuleManager::instance().removeChildDataSource(oldChild);
              }
            }
          });

  connect(pipelineFuture, &Pipeline::Future::finished, this,
          &Pipeline::finished);

  return pipelineFuture;
}

void Pipeline::startedEditingOp(Operator* op)
{
  ++m_editingOperators;
  op->setEditing();
}

void Pipeline::finishedEditingOp(Operator* op)
{
  if (op->isModified()) {
    op->resetState();
  } else {
    op->setComplete();
  }

  if (m_editingOperators > 0) {
    --m_editingOperators;
    if (m_editingOperators == 0 && !isRunning()) {
      execute(op->dataSource())->deleteWhenFinished();
    }
  }
}

bool Pipeline::beingEdited(DataSource* ds) const
{
  // If any operators in the pipeline are in editing state,
  // don't execute the pipeline
  if (ds == nullptr) {
    return false;
  }
  auto operators = ds->operators();
  for (auto itr = operators.begin(); itr != operators.end(); ++itr) {
    auto currentOp = *itr;
    if (currentOp->isEditing()) {
      return true;
    }
    // Also check if the operators in a branch are being edited.
    if (beingEdited(currentOp->childDataSource())) {
      return true;
    }
  }
  return false;
}

bool Pipeline::isModified(DataSource* datasource, Operator** start) const
{
  // If the m_operatorsDeleted flag is tripped
  // (i.e. if one or more operator were deleted since the last execution)
  // we should execute the pipeline even if no operators are in a modified state
  if (m_operatorsDeleted) {
    return true;
  }
  // If no operators are in a modified state,
  // there is no need to execute the pipeline
  if (datasource == nullptr) {
    return false;
  }
  auto operators = datasource->operators();
  for (auto itr = operators.begin(); itr != operators.end(); ++itr) {
    auto currentOp = *itr;
    if (currentOp->isModified()) {
      *start = currentOp;
      return true;
    }
    // Also check if the operators in a branch are being edited.
    if (isModified(currentOp->childDataSource(), start)) {
      return true;
    }
  }
  return false;
}

void Pipeline::branchFinished()
{
  auto future = qobject_cast<Pipeline::Future*>(sender());
  auto operators = future->operators();
  // operators will be empty if the original data source was returned as the
  // result. i.e. no operators have been run.
  if (operators.isEmpty()) {
    return;
  }
  auto start = operators.first()->dataSource();
  auto newData = future->result();

  // Set the output data on the final operator's child data source
  auto lastOp = start->operators().last();
  QString label = "Output";

  auto child = lastOp->childDataSource();
  if (child) {
    // Remove the old child, and create a new one from the output data.
    // We will always use the output data for the final output.
    label = child->label();
    ModuleManager::instance().removeChildDataSource(child);
    lastOp->setChildDataSource(nullptr);
  }

  // Remove any children produced by the operators. We currently do not
  // support intermediate data sources.
  for (auto op : operators) {
    if (op->childDataSource()) {
      ModuleManager::instance().removeChildDataSource(op->childDataSource());
      op->setChildDataSource(nullptr);
    }
  }

  // Create one
  child = new DataSource(label);
  child->setPersistenceState(tomviz::DataSource::PersistenceState::Transient);
  lastOp->setChildDataSource(child);

  // TODO: the following should be set to this, once we get
  // intermediate datasets working.
  // child->setForkable(hasChildDataSource());
  child->setForkable(false);

  // TODO: when we get intermediate datasets working, this data source
  // should have the same pipeline, with pauses in the pipeline at
  // forkable data sources.
  child->setParent(this);

  auto rootDataSource = dataSource();
  // connect signal to flow units and spacing to child data source.
  connect(dataSource(), &DataSource::dataPropertiesChanged, child,
          [rootDataSource, child]() {
            // Only flow the properties if no user modifications have been
            // made.
            if (!child->unitsModified()) {
              child->setUnits(rootDataSource->getUnits(), false);
              double spacing[3];
              rootDataSource->getSpacing(spacing);
              child->setSpacing(spacing, false);
            }
          });

  DataSource::DataSourceType type = DataSource::hasTiltAngles(newData)
                                      ? DataSource::TiltSeries
                                      : DataSource::Volume;
  child->setData(newData);
  child->setType(type);
  child->dataModified();

  emit lastOp->newChildDataSource(child);
}

void Pipeline::pause()
{
  m_paused = true;
}

bool Pipeline::paused() const
{
  return m_paused;
}

void Pipeline::resume()
{
  m_paused = false;
}

void Pipeline::cancel(std::function<void()> canceled)
{
  m_executor->cancel(canceled);
}

bool Pipeline::isRunning()
{
  return m_executor->isRunning();
}

DataSource* Pipeline::findTransformedDataSource(DataSource* dataSource)
{
  auto op = findTransformedDataSourceOperator(dataSource);
  if (op != nullptr) {
    return op->childDataSource();
  }

  return nullptr;
}

Operator* Pipeline::findTransformedDataSourceOperator(DataSource* dataSource)
{
  if (dataSource == nullptr) {
    return nullptr;
  }

  auto operators = dataSource->operators();
  for (auto itr = operators.rbegin(); itr != operators.rend(); ++itr) {
    auto op = *itr;
    if (op->childDataSource() != nullptr) {
      auto child = op->childDataSource();
      // If the child has operators we need to go deeper
      if (!child->operators().isEmpty()) {
        return findTransformedDataSourceOperator(child);
      }

      return op;
    }
  }

  return nullptr;
}

void Pipeline::addDataSource(DataSource* dataSource)
{
  connect(dataSource, &DataSource::operatorAdded, [this](Operator* op) {
    execute(op->dataSource())->deleteWhenFinished();
  });
  // Wire up transformModified to execute pipeline
  connect(dataSource, &DataSource::operatorAdded, [this](Operator* op) {
    // Extract out source and execute all.
    connect(op, &Operator::transformModified, this,
            [this]() { execute()->deleteWhenFinished(); });

    // Ensure that new child data source signals are correctly wired up.
    connect(op,
            static_cast<void (Operator::*)(DataSource*)>(
              &Operator::newChildDataSource),
            [this](DataSource* ds) { addDataSource(ds); });

    // This might also be the root datasource, which is okay
    auto oldChild = transformedDataSource(op->dataSource());

    // This is just to make the modules "move down" in the view before
    // the operator is ran.
    DataSource* outputDS = nullptr;
    auto transformedDataSourceOp =
      findTransformedDataSourceOperator(op->dataSource());
    if (transformedDataSourceOp)
      outputDS = oldChild;

    emit operatorAdded(op, outputDS);
  });
  // Wire up operatorRemoved. TODO We need to check the branch of the
  // pipeline we are currently executing.
  connect(dataSource, &DataSource::operatorRemoved, [this](Operator* op) {
    // If an operator has been removed, there's a chance that none of the
    // remaining operators are in a modified state.
    // But the pipeline should still be executed to reflect changes
    if (!op->isNew()) {
      m_operatorsDeleted = true;
    }

    if (op->dataSource()->operators().isEmpty() && op->childDataSource()) {
      // The last operator was removed. Move all the modules to the root.
      ModuleManager::instance().moveModules(op->childDataSource(),
                                            op->dataSource());
      ModuleManager::instance().removeChildDataSource(op->childDataSource());
    } else if (op->childDataSource()) {
      // Save this so we can move the modules from it and delete it
      // later.
      m_lastOperatorChildRemoved = op->childDataSource();
    }

    // If pipeline is running see if we can safely remove the operator
    if (isRunning()) {
      // If we can't safely cancel the execution then trigger the rerun of the
      // pipeline.
      if (!m_executor->cancel(op)) {
        execute(op->dataSource())->deleteWhenFinished();
      }
    } else {
      // Trigger the pipeline to run
      execute(op->dataSource())->deleteWhenFinished();
    }
  });
}

void Pipeline::addDefaultModules(DataSource* dataSource)
{
  // Note: In the future we can pull this out into a setting.
  QStringList defaultModules = { "Outline", "Slice" };
  bool oldMoveObjectsEnabled = ActiveObjects::instance().moveObjectsEnabled();
  ActiveObjects::instance().setMoveObjectsMode(false);
  auto view = ActiveObjects::instance().activeView();

  if (view == nullptr || !view->IsA("vtkSMRenderViewProxy")) {
    return;
  }

  Module* module = nullptr;
  foreach (QString name, defaultModules) {
    module =
      ModuleManager::instance().createAndAddModule(name, dataSource, view);
  }
  ActiveObjects::instance().setActiveModule(module);
  ActiveObjects::instance().setMoveObjectsMode(oldMoveObjectsEnabled);

  auto pqview = tomviz::convert<pqView*>(view);
  pqview->resetDisplay();
  pqview->render();
}

DataSource* Pipeline::transformedDataSource(DataSource* ds)
{
  if (ds == nullptr) {
    ds = dataSource();
  }

  auto transformed = findTransformedDataSource(ds);
  if (transformed != nullptr) {
    return transformed;
  }

  // Default to dataSource at being of pipeline
  return ds;
}

void Pipeline::setExecutionMode(ExecutionMode executor)
{
  m_executionMode = executor;
  if (executor == ExecutionMode::Docker) {
    m_executor.reset(new DockerPipelineExecutor(this));
  } else if (executor == ExecutionMode::Threaded) {
    m_executor.reset(new ThreadPipelineExecutor(this));
  } else if (executor == ExecutionMode::ExternalPython) {
    m_executor.reset(new ExternalPythonExecutor(this));
  } else {
    qCritical() << "Unknown executor type: " << executor;
  }
}

void Pipeline::Future::deleteWhenFinished()
{
  connect(this, &Pipeline::Future::finished, this, [this]() { deleteLater(); });
}

Pipeline::Future* Pipeline::emptyFuture()
{
  auto future = new Pipeline::Future();
  // Delay emitting signal until next event loop
  QTimer::singleShot(0, [future] { emit future->finished(); });

  return future;
}

#include "Pipeline.moc"

} // namespace tomviz

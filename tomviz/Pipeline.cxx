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
#include "Pipeline.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "DockerUtilities.h"
#include "EmdFormat.h"
#include "ModuleManager.h"
#include "Operator.h"
#include "PipelineExecutor.h"
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

void Pipeline::execute()
{
  execute(m_data);
}

void Pipeline::execute(DataSource* dataSource, Operator* start)
{
  if (paused()) {
    return;
  }
  emit started();

  if (dataSource == nullptr) {
    dataSource = m_data;
  }

  auto operators = dataSource->operators();
  if (operators.isEmpty()) {
    emit finished();
    return;
  }
  int startIndex = 0;
  // We currently only support running the last operator or the entire pipeline.
  if (start == dataSource->operators().last()) {
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
      dataSource = transformedDataSource(dataSource);
    }
  }

  m_executor->execute(dataSource->dataObject(), operators, startIndex);
}

void Pipeline::branchFinished(DataSource* start, vtkDataObject* newData)
{
  // We only add the transformed child data source if the last operator
  // doesn't already have an explicit child data source i.e.
  // hasChildDataSource is true.
  auto lastOp = start->operators().last();
  if (!lastOp->hasChildDataSource()) {
    DataSource* newChildDataSource = nullptr;
    if (lastOp->childDataSource() == nullptr) {
      newChildDataSource = new DataSource("Output");
      newChildDataSource->setPersistenceState(
        tomviz::DataSource::PersistenceState::Transient);
      newChildDataSource->setForkable(false);
      newChildDataSource->setParent(this);
      lastOp->setChildDataSource(newChildDataSource);
      auto rootDataSource = dataSource();
      // connect signal to flow units and spacing to child data source.
      connect(dataSource(), &DataSource::dataPropertiesChanged,
              [rootDataSource, newChildDataSource]() {
                // Only flow the properties if no user modifications have been
                // made.
                if (!newChildDataSource->unitsModified()) {
                  newChildDataSource->setUnits(rootDataSource->getUnits(),
                                               false);
                  double spacing[3];
                  rootDataSource->getSpacing(spacing);
                  newChildDataSource->setSpacing(spacing, false);
                }
              });
    }

    lastOp->childDataSource()->setData(newData);
    lastOp->childDataSource()->dataModified();

    if (newChildDataSource != nullptr) {
      emit lastOp->newChildDataSource(newChildDataSource);
      // Move modules from root data source.
      bool oldMoveObjectsEnabled =
        ActiveObjects::instance().moveObjectsEnabled();
      ActiveObjects::instance().setMoveObjectsMode(false);
      auto view = ActiveObjects::instance().activeView();
      foreach (Module* module, ModuleManager::instance().findModules<Module*>(
                                 dataSource(), nullptr)) {
        // TODO: We should really copy the module properties as well.
        auto newModule = ModuleManager::instance().createAndAddModule(
          module->label(), newChildDataSource, view);
        // Copy over properties using the serialization code.
        newModule->deserialize(module->serialize());
        ModuleManager::instance().removeModule(module);
      }
      ActiveObjects::instance().setMoveObjectsMode(oldMoveObjectsEnabled);
    }
  }
}

void Pipeline::pause()
{
  m_paused = true;
}

bool Pipeline::paused()
{
  return m_paused;
}

void Pipeline::resume(bool run)
{
  m_paused = false;
  if (run) {
    execute();
  }
}

void Pipeline::resume(DataSource* at)
{
  m_paused = false;
  execute(at);
}

void Pipeline::cancel(std::function<void()> canceled)
{
  m_executor->cancel(canceled);
}

bool Pipeline::isRunning()
{
  return m_executor->isRunning();
}

bool Pipeline::isPaused() const
{
  return m_paused;
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
  connect(dataSource, &DataSource::operatorAdded,
          [this](Operator* op) { execute(op->dataSource(), op); });
  // Wire up transformModified to execute pipeline
  connect(dataSource, &DataSource::operatorAdded, [this](Operator* op) {
    // Extract out source and execute all.
    connect(op, &Operator::transformModified, this, [this]() { execute(); });

    // Ensure that new child data source signals are correctly wired up.
    connect(op,
            static_cast<void (Operator::*)(DataSource*)>(
              &Operator::newChildDataSource),
            [this](DataSource* ds) { addDataSource(ds); });

    // We need to ensure we move add datasource to the end of the branch
    auto operators = op->dataSource()->operators();
    if (operators.size() > 1) {
      auto transformedDataSourceOp =
        findTransformedDataSourceOperator(op->dataSource());
      if (transformedDataSourceOp != nullptr) {
        auto transformedDataSource = transformedDataSourceOp->childDataSource();
        transformedDataSourceOp->setChildDataSource(nullptr);
        op->setChildDataSource(transformedDataSource);
        emit operatorAdded(op, transformedDataSource);
      } else {
        emit operatorAdded(op);
      }
    } else {
      emit operatorAdded(op);
    }
  });
  // Wire up operatorRemoved. TODO We need to check the branch of the
  // pipeline we are currently executing.
  connect(dataSource, &DataSource::operatorRemoved, [this](Operator* op) {
    // Do we need to move the transformed data source, !hasChildDataSource as we
    // don't want to move "explicit" child data sources.
    if (!op->hasChildDataSource() && op->childDataSource() != nullptr) {
      auto transformedDataSource = op->childDataSource();
      auto operators = op->dataSource()->operators();
      // We have an operator to move it to.
      if (!operators.isEmpty()) {
        auto newOp = operators.last();
        op->setChildDataSource(nullptr);
        newOp->setChildDataSource(transformedDataSource);
        emit newOp->dataSourceMoved(transformedDataSource);
      }
      // Clean it up
      else {
        transformedDataSource->removeAllOperators();
        transformedDataSource->deleteLater();
      }
    }

    // If pipeline is running see if we can safely remove the operator
    if (isRunning()) {
      // If we can't safely cancel the execution then trigger the rerun of the
      // pipeline.
      if (!m_executor->cancel(op)) {
        execute(op->dataSource());
      }
    } else {
      // Trigger the pipeline to run
      execute(op->dataSource());
    }
  });
}

void Pipeline::addDefaultModules(DataSource* dataSource)
{
  // Note: In the future we can pull this out into a setting.
  QStringList defaultModules = { "Outline", "Orthogonal Slice" };
  bool oldMoveObjectsEnabled = ActiveObjects::instance().moveObjectsEnabled();
  ActiveObjects::instance().setMoveObjectsMode(false);
  auto view = ActiveObjects::instance().activeView();

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

Pipeline::ImageFuture::ImageFuture(Operator* op, vtkImageData* imageData,
                                   PipelineWorker::Future* future,
                                   QObject* parent)
  : QObject(parent), m_operator(op), m_imageData(imageData), m_future(future)
{

  if (m_future != nullptr) {
    connect(m_future, &PipelineWorker::Future::finished, this,
            &Pipeline::ImageFuture::finished);
    connect(m_future, &PipelineWorker::Future::canceled, this,
            &Pipeline::ImageFuture::canceled);
  }
}

Pipeline::ImageFuture::~ImageFuture()
{
  if (m_future != nullptr) {
    m_future->deleteLater();
  }
}

Pipeline::ImageFuture* Pipeline::getCopyOfImagePriorTo(Operator* op)
{
  return m_executor->getCopyOfImagePriorTo(op);
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
  } else {
    m_executor.reset(new ThreadPipelineExecutor(this));
  }
}

} // namespace tomviz

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineManager_h
#define tomvizPipelineManager_h

#include <QObject>

#include "Pipeline.h"

class QDir;

namespace tomviz {

class Pipeline;

class PipelineManager : public QObject
{
  Q_OBJECT

public:
  static PipelineManager& instance();

  /// Update the execution mode the pipelines are using.
  void updateExecutionMode(Pipeline::ExecutionMode mode);
  Pipeline::ExecutionMode executionMode();
  QList<QPointer<Pipeline>>& pipelines();

public slots:
  void addPipeline(Pipeline*);
  void removePipeline(Pipeline*);
  void removeAllPipelines();

signals:
  void executionModeUpdated(Pipeline::ExecutionMode mode);

private:
  Q_DISABLE_COPY(PipelineManager)
  PipelineManager(QObject* parent = nullptr);
  ~PipelineManager() override;

  QList<QPointer<Pipeline>> m_pipelines;
  Pipeline::ExecutionMode m_executionMode;
};
} // namespace tomviz

#endif

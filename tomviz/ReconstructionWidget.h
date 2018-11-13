/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizReconstructionWidget_h
#define tomvizReconstructionWidget_h

#include <QWidget>

namespace tomviz {
class DataSource;

class ReconstructionWidget : public QWidget
{
  Q_OBJECT

public:
  ReconstructionWidget(DataSource* source, QWidget* parent = nullptr);
  ~ReconstructionWidget() override;

public slots:
  void startReconstruction();
  void updateProgress(int progress);
  void updateIntermediateResults(std::vector<float> reconSlice);

signals:
  void reconstructionFinished();
  void reconstructionCancelled();

private:
  Q_DISABLE_COPY(ReconstructionWidget)

  class RWInternal;
  RWInternal* Internals;
};
} // namespace tomviz

#endif

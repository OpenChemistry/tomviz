/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizReconstructionOperator_h
#define tomvizReconstructionOperator_h

#include "Operator.h"

namespace tomviz {
class DataSource;

class ReconstructionOperator : public Operator
{
  Q_OBJECT

public:
  ReconstructionOperator(DataSource* source, QObject* parent = nullptr);

  QString label() const override { return "Reconstruction"; }

  QIcon icon() const override;

  Operator* clone() const override;

  QWidget* getCustomProgressWidget(QWidget*) const override;

protected:
  bool applyTransform(vtkDataObject* data) override;

signals:
  /// Emitted after each slice is reconstructed, use to display intermediate
  /// results the first vector contains the sinogram reconstructed, the second
  /// contains the slice of the resulting image.
  void intermediateResults(std::vector<float> resultSlice);

private:
  DataSource* m_dataSource;
  int m_extent[6];
  Q_DISABLE_COPY(ReconstructionOperator)
};
} // namespace tomviz

#endif

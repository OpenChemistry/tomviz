/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSnapshotOperator_h
#define tomvizSnapshotOperator_h

#include "Operator.h"

namespace tomviz {
class DataSource;

class SnapshotOperator : public Operator
{
  Q_OBJECT

public:
  SnapshotOperator(DataSource* source, QObject* parent = nullptr);

  QString label() const override { return "Snapshot"; }

  QIcon icon() const override;

  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  QWidget* getCustomProgressWidget(QWidget*) const override;

protected:
  bool applyTransform(vtkDataObject* data) override;

private:
  DataSource* m_dataSource;
  bool m_updateCache = true; // Update the first time, then freeze.
  Q_DISABLE_COPY(SnapshotOperator)
};
} // namespace tomviz

#endif

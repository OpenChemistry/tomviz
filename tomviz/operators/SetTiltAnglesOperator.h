/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizSetTiltAnglesOperator_h
#define tomvizSetTiltAnglesOperator_h

#include "Operator.h"

#include <QMap>

namespace tomviz {

class SetTiltAnglesOperator : public Operator
{
  Q_OBJECT

public:
  SetTiltAnglesOperator(QObject* parent = nullptr);

  QString label() const override { return "Set Tilt Angles"; }
  QIcon icon() const override;
  Operator* clone() const override;
  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;
  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;
  bool hasCustomUI() const override { return true; }

  void setTiltAngles(const QMap<size_t, double>& newAngles);
  const QMap<size_t, double>& tiltAngles() const { return m_tiltAngles; }

protected:
  bool applyTransform(vtkDataObject* data) override;

  QMap<size_t, double> m_tiltAngles;

private:
  Q_DISABLE_COPY(SetTiltAnglesOperator)
};
} // namespace tomviz

#endif

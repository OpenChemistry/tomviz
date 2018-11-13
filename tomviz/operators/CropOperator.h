/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCropOperator_h
#define tomvizCropOperator_h

#include "Operator.h"

namespace tomviz {

class CropOperator : public Operator
{
  Q_OBJECT

public:
  CropOperator(QObject* parent = nullptr);

  QString label() const override { return "Crop"; }

  QIcon icon() const override;

  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;
  bool hasCustomUI() const override { return true; }

  void setCropBounds(const int bounds[6]);
  const int* cropBounds() const { return m_bounds; }

protected:
  bool applyTransform(vtkDataObject* data) override;

private:
  int m_bounds[6];
  Q_DISABLE_COPY(CropOperator)
};
} // namespace tomviz

#endif

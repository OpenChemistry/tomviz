/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTransposeDataOperator_h
#define tomvizTransposeDataOperator_h

#include "Operator.h"

namespace tomviz {

class TransposeDataOperator : public Operator
{
  Q_OBJECT

public:
  TransposeDataOperator(QObject* parent = nullptr);

  QString label() const override { return "Transpose Data"; }
  QIcon icon() const override;
  Operator* clone() const override;

  bool applyTransform(vtkDataObject* data) override;

  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;
  bool hasCustomUI() const override { return true; }

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  enum class TransposeType
  {
    C,
    Fortran
  };

  void setTransposeType(TransposeType t) { m_transposeType = t; }

private:
  TransposeType m_transposeType = TransposeType::C;

  Q_DISABLE_COPY(TransposeDataOperator)
};
} // namespace tomviz

#endif

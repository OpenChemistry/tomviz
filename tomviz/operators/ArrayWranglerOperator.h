/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizArrayWranglerOperator_h
#define tomvizArrayWranglerOperator_h

#include "Operator.h"

namespace tomviz {

class ArrayWranglerOperator : public Operator
{
  Q_OBJECT

public:
  ArrayWranglerOperator(QObject* parent = nullptr);

  QString label() const override { return "Wrangle Data"; }
  QIcon icon() const override;
  Operator* clone() const override;

  bool applyTransform(vtkDataObject* data) override;

  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;
  bool hasCustomUI() const override { return true; }

  enum class OutputType
  {
    UInt8,
    UInt16
  };

  void setOutputType(OutputType t) { m_outputType = t; }
  void setComponentToKeep(size_t i) { m_componentToKeep = i; }

private:
  OutputType m_outputType;
  int m_componentToKeep;

  Q_DISABLE_COPY(ArrayWranglerOperator)
};
} // namespace tomviz

#endif

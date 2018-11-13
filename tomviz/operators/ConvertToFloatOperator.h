/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConvertToFloatOperator_h
#define tomvizConvertToFloatOperator_h

#include "Operator.h"

namespace tomviz {

class ConvertToFloatOperator : public Operator
{
  Q_OBJECT

public:
  ConvertToFloatOperator(QObject* parent = nullptr);

  QString label() const override { return "Convert to Float"; }
  QIcon icon() const override;
  Operator* clone() const override;

  bool applyTransform(vtkDataObject* data) override;

private:
  Q_DISABLE_COPY(ConvertToFloatOperator)
};
} // namespace tomviz

#endif

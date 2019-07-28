/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizConvertToVolumeOperator_h
#define tomvizConvertToVolumeOperator_h

#include "Operator.h"

namespace tomviz
{

class ConvertToVolumeOperator : public Operator
{
  Q_OBJECT

public:
  ConvertToVolumeOperator(
    QObject* p = nullptr,
    DataSource::DataSourceType t = DataSource::Volume,
    QString label = "Mark as Volume");
  ~ConvertToVolumeOperator();

  QString label() const override { return m_label; }
  QIcon icon() const override;
  Operator* clone() const override;

protected:
  bool applyTransform(vtkDataObject* data) override;

private:
  Q_DISABLE_COPY(ConvertToVolumeOperator)
  DataSource::DataSourceType m_type;
  QString m_label;
};

} // end tomviz namespace

#endif

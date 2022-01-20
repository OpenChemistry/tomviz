/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizTimeSeriesLabel_h
#define tomvizTimeSeriesLabel_h

#include <QObject>

namespace tomviz {

class TimeSeriesLabel : public QObject
{
  Q_OBJECT

public:
  TimeSeriesLabel(QObject* parent);
  ~TimeSeriesLabel();

private:
  Q_DISABLE_COPY(TimeSeriesLabel)

  class Internal;
  QScopedPointer<Internal> m_internal;
};
} // namespace tomviz

#endif

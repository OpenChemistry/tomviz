/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPyXRFRunner_h
#define tomvizPyXRFRunner_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class PyXRFRunner : public QObject
{
  Q_OBJECT

public:
  explicit PyXRFRunner(QObject* parent);
  ~PyXRFRunner() override;

  // Returns True if the necessary dependencies are installed
  bool isInstalled();

  void start();

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};
} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPtychoRunner_h
#define tomvizPtychoRunner_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class PtychoRunner : public QObject
{
  Q_OBJECT

public:
  explicit PtychoRunner(QObject* parent);
  ~PtychoRunner() override;

  // Returns True if the necessary dependencies are installed
  bool isInstalled();

  void start();

  // Whether to auto-load the final dataset after it finishes
  void setAutoLoadFinalData(bool b);

  // Get the import error if the needed modules are not installed
  QString importError();

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};
} // namespace tomviz

#endif

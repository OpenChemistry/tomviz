/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizProgressDialogManager_h
#define tomvizProgressDialogManager_h

#include <QObject>

class QMainWindow;

namespace tomviz {
class Operator;
class DataSource;

class ProgressDialogManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  ProgressDialogManager(QMainWindow* mw);
  virtual ~ProgressDialogManager();

private slots:
  void operationStarted();
  void operationProgress(int progress);
  void operatorAdded(Operator* op);
  void dataSourceAdded(DataSource* ds);
  void showStatusBarMessage(const QString& message);

private:
  QMainWindow* mainWindow;
  Q_DISABLE_COPY(ProgressDialogManager)
};
} // namespace tomviz
#endif

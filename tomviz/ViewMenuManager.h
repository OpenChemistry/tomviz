/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizViewMenuManager_h
#define tomvizViewMenuManager_h

#include <pqViewMenuManager.h>

#include <QPointer>
#include <QScopedPointer>

class QDialog;
class QAction;

class vtkSMViewProxy;

namespace tomviz {

class DataSource;
class SliceViewDialog;

enum class ScaleLegendStyle : unsigned int;

class ViewMenuManager : public pqViewMenuManager
{
  Q_OBJECT
public:
  ViewMenuManager(QMainWindow* mainWindow, QMenu* menu);
  ~ViewMenuManager();

private slots:
  void setProjectionModeToPerspective();
  void setProjectionModeToOrthographic();
  void onViewPropertyChanged();
  void onViewChanged();

  void setShowCenterAxes(bool show);
  void setShowOrientationAxes(bool show);

  void showDarkWhiteData();

private:
  void setScaleLegendStyle(ScaleLegendStyle);
  void setScaleLegendVisibility(bool);

  void updateDataSource(DataSource* s);
  void updateDataSourceEnableStates();

  QPointer<QAction> m_perspectiveProjectionAction;
  QPointer<QAction> m_orthographicProjectionAction;
  QPointer<QAction> m_showCenterAxesAction;
  QPointer<QAction> m_showOrientationAxesAction;
  QPointer<QAction> m_showDarkWhiteDataAction;

  QScopedPointer<SliceViewDialog> m_sliceViewDialog;

  DataSource* m_dataSource = nullptr;
  vtkSMViewProxy* m_view;
  unsigned long m_viewObserverId;
};
} // namespace tomviz

#endif

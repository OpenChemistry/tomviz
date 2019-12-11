/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizViewMenuManager_h
#define tomvizViewMenuManager_h

#include <vtkNew.h>

#include <pqViewMenuManager.h>

#include <QPointer>
#include <QScopedPointer>

class QDialog;
class QAction;

class vtkImageSlice;
class vtkImageSliceMapper;
class vtkRenderer;
class vtkSMViewProxy;

namespace tomviz {

class DataSource;

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

  void showDarkData();
  void showWhiteData();

private:
  void setScaleLegendStyle(ScaleLegendStyle);
  void setScaleLegendVisibility(bool);

  void updateDataSource(DataSource* s);
  void updateDataSourceEnableStates();

  QPointer<QAction> m_perspectiveProjectionAction;
  QPointer<QAction> m_orthographicProjectionAction;
  QPointer<QAction> m_showCenterAxesAction;
  QPointer<QAction> m_showOrientationAxesAction;
  QPointer<QAction> m_showDarkDataAction;
  QPointer<QAction> m_showWhiteDataAction;

  QScopedPointer<QDialog> m_darkDataDialog;
  QScopedPointer<QDialog> m_whiteDataDialog;

  vtkNew<vtkImageSliceMapper> m_darkImageSliceMapper;
  vtkNew<vtkImageSliceMapper> m_whiteImageSliceMapper;

  vtkNew<vtkImageSlice> m_darkImageSlice;
  vtkNew<vtkImageSlice> m_whiteImageSlice;

  vtkNew<vtkRenderer> m_darkRenderer;
  vtkNew<vtkRenderer> m_whiteRenderer;

  DataSource* m_dataSource = nullptr;
  vtkSMViewProxy* m_view;
  unsigned long m_viewObserverId;
};
} // namespace tomviz

#endif

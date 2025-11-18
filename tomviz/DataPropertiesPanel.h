/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataPropertiesPanel_h
#define tomvizDataPropertiesPanel_h

#include <QWidget>

#include "DataPropertiesModel.h"

#include <QPointer>
#include <QScopedPointer>

class pqProxyWidget;
class QComboBox;
class QLineEdit;
class QTableView;
class vtkPVDataInformation;

namespace Ui {
class DataPropertiesPanel;
}

namespace tomviz {

class DataSource;
class ListEditorDialog;

/// DataPropertiesPanel is the panel that shows information (and other controls)
/// for a DataSource. It monitors tomviz::ActiveObjects instance and shows
/// information about the active data source, as well allow the user to edit
/// configurable options, such as color map.
class DataPropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  explicit DataPropertiesPanel(QWidget* parent = nullptr);
  ~DataPropertiesPanel() override;

  bool eventFilter(QObject*, QEvent*) override;

protected:
  void paintEvent(QPaintEvent*) override;
  void updateData();
  void updateTimeSeriesGroup();
  void updateComponentsCombo();
  static bool parseField(QLineEdit* widget, double& value);
  void updateLength(QLineEdit* widget, int axis);
  void updateVoxelSize(QLineEdit* widget, int axis);
  void updateOrigin(QLineEdit* widget, int axis);
  void updateOrientation(QLineEdit* widget, int axis);

private slots:
  void setDataSource(DataSource*);
  void onTiltAnglesModified(int row, int column);
  void setTiltAngles();
  void scheduleUpdate();
  void onDataPropertiesChanged();
  void onDataPositionChanged(double, double, double);
  void onDataOrientationChanged(double, double, double);
  void editTimeSeries();

  void updateUnits();

  void updateAxesGridLabels();

  void setActiveScalars(const QString& activeScalars);

  void componentNameEdited(int index, const QString& name);

signals:
  void colorMapUpdated();

private:
  Q_DISABLE_COPY(DataPropertiesPanel)

  bool m_updateNeeded = true;
  QScopedPointer<Ui::DataPropertiesPanel> m_ui;
  QPointer<DataSource> m_currentDataSource;
  QPointer<pqProxyWidget> m_colorMapWidget;
  QPointer<QWidget> m_tiltAnglesSeparator;
  QPointer<QWidget> m_timeSeriesSeparator;
  QPointer<ListEditorDialog> m_timeSeriesEditor;
  DataPropertiesModel m_scalarsTableModel;
  // Hold the order (the indexes into the field data), so we can preserve
  // the order during a rename.
  QList<int> m_scalarIndexes;

  void clear();
  void updateSpacing(int axis, double newLength);
  QList<ArrayInfo> getArraysInfo(DataSource* dataSource);
  void updateInformationWidget(QTableView* scalarsTable,
                               const QList<ArrayInfo>& arraysInfo);
  void updateActiveScalarsCombo(QComboBox* scalarsCombo,
                                const QList<ArrayInfo>& arraysInfo);
  static void resetCamera();
};
} // namespace tomviz

#endif

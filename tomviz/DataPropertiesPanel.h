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
class QTableView;
class vtkPVDataInformation;

namespace Ui {
class DataPropertiesPanel;
}

namespace tomviz {

class DataSource;

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

private slots:
  void setDataSource(DataSource*);
  void onTiltAnglesModified(int row, int column);
  void setTiltAngles();
  void scheduleUpdate();

  void updateUnits();
  void updateXLength();
  void updateYLength();
  void updateZLength();

  void updateAxesGridLabels();

  void setActiveScalars(QString activeScalars);

signals:
  void colorMapUpdated();

private:
  Q_DISABLE_COPY(DataPropertiesPanel)

  bool m_updateNeeded = true;
  QScopedPointer<Ui::DataPropertiesPanel> m_ui;
  QPointer<DataSource> m_currentDataSource;
  QPointer<pqProxyWidget> m_colorMapWidget;
  QPointer<QWidget> m_tiltAnglesSeparator;
  DataPropertiesModel m_scalarsTableModel;

  void clear();
  void updateSpacing(int axis, double newLength);
  QList<ArrayInfo> getArraysInfo(vtkPVDataInformation* dataInfo) const;
  void updateInformationWidget(QTableView* scalarsTable,
                               QList<ArrayInfo> arraysInfo);
  void updateActiveScalarsCombo(QComboBox* scalarsCombo,
                                QList<ArrayInfo> arraysInfo);
  static void resetCamera();
};
} // namespace tomviz

#endif

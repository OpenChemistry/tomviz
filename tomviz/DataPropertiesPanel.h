/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef tomvizDataPropertiesPanel_h
#define tomvizDataPropertiesPanel_h

#include <QWidget>

#include <QPointer>
#include <QScopedPointer>

class pqProxyWidget;

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

signals:
  void colorMapUpdated();

private:
  Q_DISABLE_COPY(DataPropertiesPanel)

  bool m_updateNeeded = true;
  QScopedPointer<Ui::DataPropertiesPanel> m_ui;
  QPointer<DataSource> m_currentDataSource;
  QPointer<pqProxyWidget> m_colorMapWidget;
  QPointer<QWidget> m_tiltAnglesSeparator;

  void clear();
  void updateSpacing(int axis, double newLength);
};
}

#endif

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

#include <QScopedPointer>
#include <QWidget>

namespace tomviz {

class DataSource;

/// DataPropertiesPanel is the panel that shows information (and other controls)
/// for a DataSource. It monitors tomviz::ActiveObjects instance and shows
/// information about the active data source, as well allow the user to edit
/// configurable options, such as color map.
class DataPropertiesPanel : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  explicit DataPropertiesPanel(QWidget* parent = nullptr);
  virtual ~DataPropertiesPanel() override;

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

signals:
  void colorMapUpdated();

private:
  Q_DISABLE_COPY(DataPropertiesPanel)

  class DPPInternals;
  const QScopedPointer<DPPInternals> Internals;

  bool updateNeeded = true;
};
}

#endif

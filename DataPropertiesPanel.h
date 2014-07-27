/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __TEM_DataPropertiesPanel_h
#define __TEM_DataPropertiesPanel_h

#include <QWidget>
#include <QScopedPointer>

namespace TEM
{

class DataSource;

/// DataPropertiesPanel is the panel that shows information (and other controls)
/// for a DataSource. It monitors TEM::ActiveObjects instance and shows
/// information about the active data source, as well allow the user to edit
/// configurable options, such as color map.
class DataPropertiesPanel : public QWidget
{
  Q_OBJECT;
  typedef QWidget Superclass;
public:
  DataPropertiesPanel(QWidget* parent=0);
  virtual ~DataPropertiesPanel();

private slots:
  void setDataSource(DataSource*);
  void update();

private:
  Q_DISABLE_COPY(DataPropertiesPanel);

  class DPPInternals;
  const QScopedPointer<DPPInternals> Internals;
};

}

#endif

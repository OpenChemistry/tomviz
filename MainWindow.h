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
#ifndef __MainWindow_h
#define __MainWindow_h

#include <QMainWindow>

namespace TEM
{

class DataSource;
class Module;

/// The main window for the TomViz application.
class MainWindow : public QMainWindow
{
  Q_OBJECT

  typedef QMainWindow Superclass;

public:
  MainWindow(QWidget* parent=0, Qt::WindowFlags flags=0);
  virtual ~MainWindow();

private:
  Q_DISABLE_COPY(MainWindow)
  class MWInternals;
  MWInternals* Internals;
};

}
#endif

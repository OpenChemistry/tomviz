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

#ifndef tomvizWelcomeDialog_h
#define tomvizWelcomeDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class WelcomeDialog;
}

namespace tomviz {
class MainWindow;

class WelcomeDialog : public QDialog
{
  Q_OBJECT

public:
  explicit WelcomeDialog(MainWindow* parent);
  ~WelcomeDialog() override;

private slots:
  // When user clicks yes to load sample data
  void onLoadSampleDataClicked();

  // React to checkbox events
  void onDoNotShowAgainStateChanged(int);

private:
  QScopedPointer<Ui::WelcomeDialog> m_ui;
};
} // namespace tomviz

#endif

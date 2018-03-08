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

#ifndef tomvizConnectionDialog_h
#define tomvizConnectionDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class ConnectionDialog;
}

namespace tomviz {

class ConnectionDialog : public QDialog
{
  Q_OBJECT

public:
  explicit ConnectionDialog(const QString name = "",
                            const QString hostName = "", int port = 8080,
                            QWidget* parent = nullptr);
  ~ConnectionDialog() override;

  QString name();
  QString hostName();
  int port();

private:
  QScopedPointer<Ui::ConnectionDialog> m_ui;
};
}

#endif

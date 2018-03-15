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

#ifndef tomvizRegexGroupDialog_h
#define tomvizRegexGroupDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace Ui {
class RegexGroupDialog;
}

namespace tomviz {

class RegexGroupDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RegexGroupDialog(const QString name = "", QWidget* parent = nullptr);
  ~RegexGroupDialog();

  QString name();

private:
  QScopedPointer<Ui::RegexGroupDialog> m_ui;
};
}

#endif

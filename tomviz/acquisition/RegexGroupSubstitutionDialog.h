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

#ifndef tomvizRegexGroupSubstitutionDialog_h
#define tomvizRegexGroupSubstitutionDialog_h

#include <QDialog>

#include <QLabel>
#include <QScopedPointer>

namespace Ui {
class RegexGroupSubstitutionDialog;
}

namespace tomviz {

class RegexGroupSubstitutionDialog : public QDialog
{
  Q_OBJECT

public:
  explicit RegexGroupSubstitutionDialog(const QString groupName = "",
                                        const QString regex = "",
                                        const QString substitution = "",
                                        QWidget* parent = nullptr);
  ~RegexGroupSubstitutionDialog() override;

  QString groupName();
  QString regex();
  QString substitution();

public slots:
  void done(int r) override;

private:
  QScopedPointer<Ui::RegexGroupSubstitutionDialog> m_ui;
  QLabel m_regexErrorLabel;
};
} // namespace tomviz

#endif

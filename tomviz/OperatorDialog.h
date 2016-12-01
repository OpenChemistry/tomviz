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
#ifndef tomvizOperatorDialog_h
#define tomvizOperatorDialog_h

#include <QDialog>
#include <QMap>
#include <QString>
#include <QVariant>

namespace tomviz {

class OperatorWidget;

class OperatorDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;

public:
  OperatorDialog(QWidget* parent = nullptr);
  ~OperatorDialog() override;

  /// Set the JSON description of the operator
  void setJSONDescription(const QString& json);

  /// Get parameter values
  QMap<QString, QVariant> values() const;

private:
  Q_DISABLE_COPY(OperatorDialog)
  OperatorWidget* m_ui = nullptr;
};
}

#endif

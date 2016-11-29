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
#ifndef tomvizOperatorWidget_h
#define tomvizOperatorWidget_h

#include <OperatorPython.h>

#include <QMap>
#include <QString>
#include <QVariant>
#include <QWidget>

namespace tomviz {

class OperatorWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  OperatorWidget(QWidget* parent = nullptr, OperatorPython* op = nullptr);
  ~OperatorWidget() override;

  /// Set the JSON description of the operator
  void setJSONDescription(const QString& json);

  /// Get parameter values
  QMap<QString, QVariant> values() const;

private:
  Q_DISABLE_COPY(OperatorWidget)
  OperatorPython* m_operator = nullptr;
};
}

#endif

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
#ifndef tomvizOperatorPropertiesPanel_h
#define tomvizOperatorPropertiesPanel_h

#include <QWidget>

#include <QPointer>

class QLabel;
class QVBoxLayout;

namespace tomviz {
class Operator;
class OperatorPython;
class OperatorWidget;

class OperatorPropertiesPanel : public QWidget
{
  Q_OBJECT

public:
  OperatorPropertiesPanel(QWidget* parent = nullptr);
  virtual ~OperatorPropertiesPanel();

private slots:
  void setOperator(Operator*);
  void setOperator(OperatorPython*);
  void apply();
  void viewCodePressed();

private:
  Q_DISABLE_COPY(OperatorPropertiesPanel)

  QPointer<Operator> m_activeOperator = nullptr;
  QVBoxLayout* m_layout = nullptr;
  OperatorWidget* m_operatorWidget = nullptr;
};
} // namespace tomviz

#endif

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
#ifndef tomvizOperatorsWidget_h
#define tomvizOperatorsWidget_h

#include <QTreeWidget>
#include <QScopedPointer>

namespace tomviz
{

class DataSource;
class Operator;

class OperatorsWidget : public QTreeWidget
{
  Q_OBJECT
  typedef QTreeWidget Superclass;

public:
  OperatorsWidget(QWidget* parent=nullptr);
  virtual ~OperatorsWidget();

private slots:
  void operatorAdded(QSharedPointer<Operator> &op);
//  void operatorRemoved(Operator* op);
  void onItemClicked(QTreeWidgetItem*, int);

  /// called when the current data source changes.
  void setDataSource(DataSource*);

  void updateTransforms();

  void itemDoubleClicked(QTreeWidgetItem*);

  void updateOperatorLabel();

private:
  Q_DISABLE_COPY(OperatorsWidget)

  class OWInternals;
  QScopedPointer<OWInternals> Internals;
};
}

#endif

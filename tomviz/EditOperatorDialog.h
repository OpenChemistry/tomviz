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
#ifndef tomvizEditPythonOperatorDialog_h
#define tomvizEditPythonOperatorDialog_h

#include <QDialog>
#include <QScopedPointer>
#include <QSharedPointer>

namespace tomviz
{
class Operator;
class DataSource;

class EditOperatorDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;
public:
  // Creates an editor dialog for the given operator.  If this is creating a
  // new operator, then pass in the data source that the operator needs to be
  // added to and the first time that apply or OK is clicked it will be added
  // to that data source.
  EditOperatorDialog(QSharedPointer<Operator> &op,
                     DataSource* dataSource = NULL,
                     QWidget* parent = NULL);
  virtual ~EditOperatorDialog();

  QSharedPointer<Operator>& op();

private slots:
  void onApply();

private:
  Q_DISABLE_COPY(EditOperatorDialog)
  class EODInternals;
  QScopedPointer<EODInternals> Internals;
};

}

#endif

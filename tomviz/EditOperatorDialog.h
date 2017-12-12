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

namespace tomviz {
class Operator;
class DataSource;
class EditOperatorWidget;

class EditOperatorDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;

public:
  // Creates an editor dialog for the given operator.  If this is creating a
  // new operator, then pass in true for needToAddOperator and the first time
  // Apply/Ok is pressed it will be added to the DataSource.
  EditOperatorDialog(Operator* op, DataSource* dataSource,
                     bool needToAddOperator, QWidget* parent);
  virtual ~EditOperatorDialog();

  void setViewMode(const QString& mode);

  Operator* op();

  static void showDialogForOperator(Operator* op, const QString& viewMode = "");

private slots:
  void onApply();
  void onClose();
  void getCopyOfImagePriorToFinished(bool result);

private:
  void setupUI(EditOperatorWidget* opWidget = nullptr);
  Q_DISABLE_COPY(EditOperatorDialog)
  class EODInternals;
  QScopedPointer<EODInternals> Internals;
};
}

#endif

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

  // Used to set the mode of the EditOperatorWidget in the dialog.  The mode
  // corresponds to dialog options like tabs and varies from operator to
  // operator. If the requested mode is not recognized, or the widget does not
  // support modes, this function does nothing.
  void setViewMode(const QString& mode);

  Operator* op();

  // If the given operator does not already have a dialog, this function creates
  // and shows a new dialog for that operator with the given mode (see comment
  // above on setViewMode for details about modes).  If the given operator has a
  // dialog already, that dialog is set to the requested mode and given focus.
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

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

class EditOperatorDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;
public:
  EditOperatorDialog(QSharedPointer<Operator> &op,
                           QWidget* parent = NULL);
  virtual ~EditOperatorDialog();

  QSharedPointer<Operator>& op();

signals:
  /// This signals that either Apply or OK has been pressed on
  /// the dialog.  In most cases this should be handled internally
  /// since this will call applyChangesToOperator() on the operator's
  /// editor widget before emitting this signal.  But this can
  /// be used for first-time operator creation to add the operator to
  /// the data source.
  void applyChanges();

private slots:
  void onApply();

private:
  Q_DISABLE_COPY(EditOperatorDialog)
  class EODInternals;
  QScopedPointer<EODInternals> Internals;
};

}

#endif

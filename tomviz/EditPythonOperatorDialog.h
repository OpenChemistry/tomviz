/******************************************************************************

  This source file is part of the TEM tomography project.

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

namespace TEM
{
class Operator;

class EditPythonOperatorDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;
public:
  EditPythonOperatorDialog(QSharedPointer<Operator> &op,
                           QWidget* parent = NULL);
  virtual ~EditPythonOperatorDialog();

  QSharedPointer<Operator>& op();

private slots:
  void acceptChanges();

private:
  Q_DISABLE_COPY(EditPythonOperatorDialog)
  class EPODInternals;
  QScopedPointer<EPODInternals> Internals;
};

}

#endif

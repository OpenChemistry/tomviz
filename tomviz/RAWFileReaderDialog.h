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
#ifndef tomvizRAWFileReaderDialog
#define tomvizRAWFileReaderDialog

#include <QDialog>
#include <QScopedPointer>

class vtkSMProxy;

namespace Ui {
class RAWFileReaderDialog;
}
namespace tomviz {

class RAWFileReaderDialog : public QDialog
{
  Q_OBJECT
public:
  RAWFileReaderDialog(vtkSMProxy* reader, QWidget* parent = nullptr);
  ~RAWFileReaderDialog();

  void dimensions(size_t*);
  int components();
  int vtkDataType();
private slots:
  void sanityCheckSize();
  void dataTypeChanged();
  void onAccepted();

private:
  QScopedPointer<Ui::RAWFileReaderDialog> m_ui;
  vtkSMProxy* m_reader;
  size_t m_filesize;

  bool isSigned(int vtkType);
  int vtkDataTypeToIndex(int vtkType);
};
} // namespace tomviz

#endif

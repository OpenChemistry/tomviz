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
#ifndef tomvizCropDialog_h
#define tomvizCropDialog_h

#include <QDialog>
#include <QScopedPointer>

class vtkObject;
class vtkImageData;
namespace tomviz
{

class DataSource;

class CropDialog : public QDialog
{
  Q_OBJECT
  typedef QDialog Superclass;
public:
  CropDialog(QWidget* parent, DataSource* data);
  virtual ~CropDialog();

private slots:
  void crop();
  void valueChanged();
  void cancel();

public slots:
  void updateBounds(double *bounds);

signals:
  void bounds(int *bounds);

private:
  Q_DISABLE_COPY(CropDialog)
  class CDInternals;
  QScopedPointer<CDInternals> Internals;
};

}

#endif

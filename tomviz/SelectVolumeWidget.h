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
#ifndef tomvizSelectVolumeWidget_h
#define tomvizSelectVolumeWidget_h

#include <QWidget>
#include <QScopedPointer>

class vtkObject;

namespace tomviz
{

class CropOperator;

class SelectVolumeWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  SelectVolumeWidget(const double origin[3], const double spacing[3],
                     const int extent[6], const int currentVolume[6],
                     QWidget* parent = 0);
  virtual ~SelectVolumeWidget();

  // Gets the bounds of the selection in real space (taking into account
  // the origin and spacing of the image)
  void getBoundsOfSelection(double bounds[6]);
  // Gets the bounds of the selection as extents of interest.  This is
  // the region of interest in terms of the extent given in the input
  // without the origin and spacing factored in.
  void getExtentOfSelection(int extent[6]);

private slots:
  void interactionEnd(vtkObject* caller);
  void valueChanged();
  void updateBounds(int* bounds);
  void updateBounds(double *bounds);

private:
  class CWInternals;
  QScopedPointer<CWInternals> Internals;

};

}

#endif

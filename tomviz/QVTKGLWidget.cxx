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

#include "QVTKGLWidget.h"

#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkNew.h>

#include <QSurfaceFormat>
namespace tomviz {

QVTKGLWidget::QVTKGLWidget(QWidget* parent, Qt::WindowFlags f)
  : QVTKOpenGLWidget(parent, f)
{
  // Set some defaults for our render window.
  vtkNew<vtkGenericOpenGLRenderWindow> window;
  SetRenderWindow(window);
  QSurfaceFormat glFormat = QVTKOpenGLWidget::defaultFormat();
  glFormat.setSamples(8);
  setFormat(glFormat);
}

QVTKGLWidget::~QVTKGLWidget() = default;

} // namespace tomviz

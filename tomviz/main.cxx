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
#include <QApplication>

#include <QSurfaceFormat>

#include <QDebug>

#include <pqOptions.h>
#include <pqPVApplicationCore.h>

#include <QVTKOpenGLWidget.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

#include "MainWindow.h"
#include "tomvizConfig.h"
#include "tomvizPythonConfig.h"

#include <clocale>

// We need to override the default options to enable streaming by default.
// Streaming needs to be enabled for the dax representations
class TomvizOptions : public pqOptions
{
public:
  static TomvizOptions* New();
  vtkTypeMacro(TomvizOptions, pqOptions) int GetEnableStreaming() override
  {
    return 1;
  }

protected:
  TomvizOptions() : pqOptions() { ; }
};
vtkStandardNewMacro(TomvizOptions)

  int main(int argc, char** argv)
{
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLWidget::defaultFormat());

  QCoreApplication::setApplicationName("tomviz");
  QCoreApplication::setApplicationVersion(TOMVIZ_VERSION);
  QCoreApplication::setOrganizationName("tomviz");
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  tomviz::InitializePythonEnvironment(argc, argv);

  QApplication app(argc, argv);

#if defined(__APPLE__)
  // See if this helps Python initialize itself on macOS.
  std::string exeDir = QApplication::applicationDirPath().toLatin1().data();
  if (!tomviz::isBuildDir(exeDir)) {
    QByteArray pythonPath =
      (exeDir + tomviz::PythonInitializationPythonPath()).c_str();
    qDebug() << "Setting PYTHONHOME and PYTHONPATH:" << pythonPath;
    qputenv("PYTHONPATH", pythonPath);
    qputenv("PYTHONHOME", pythonPath);
  }
#endif

  setlocale(LC_NUMERIC, "C");
  vtkNew<TomvizOptions> options;
  pqPVApplicationCore appCore(argc, argv, options.Get());
  tomviz::MainWindow window;
  window.show();
  return app.exec();
}

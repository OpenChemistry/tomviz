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

#ifdef Q_OS_WIN
#include <QStyleFactory>
#endif

#include <QSurfaceFormat>

#include <QDebug>

#include <pqPVApplicationCore.h>

#include <QVTKOpenGLWidget.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

#include "MainWindow.h"
#include "tomvizConfig.h"
#include "tomvizPythonConfig.h"

#include <clocale>

int main(int argc, char** argv)
{
  QSurfaceFormat::setDefaultFormat(QVTKOpenGLWidget::defaultFormat());

  QCoreApplication::setApplicationName("tomviz");
  QCoreApplication::setApplicationVersion(TOMVIZ_VERSION);
  QCoreApplication::setOrganizationName("tomviz");
  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

#ifdef Q_OS_WIN
  // The default changed to "windows", looked very dated...
  QApplication::setStyle(QStyleFactory::create("windowsvista"));
#endif

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
  // Set environment variable to indicate that we are running inside the Tomviz
  // application vs Python command line. This can be used to selectively load
  // modules.
  qputenv("TOMVIZ_APPLICATION", "1");

  setlocale(LC_NUMERIC, "C");
  pqPVApplicationCore appCore(argc, argv);
  tomviz::MainWindow window;
  window.show();
  return app.exec();
}

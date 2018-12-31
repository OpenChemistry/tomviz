/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>

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

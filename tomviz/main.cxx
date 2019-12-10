/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>

#include <QSplashScreen>
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

  QPixmap pixmap(":/icons/tomvizfull.png");
  QSplashScreen splash(pixmap);
  splash.show();
  app.processEvents();

  std::string exeDir = QApplication::applicationDirPath().toLatin1().data();
  if (tomviz::isApplicationBundle(exeDir)) {
    QByteArray pythonPath = tomviz::bundlePythonPath(exeDir).c_str();
    qputenv("PYTHONPATH", pythonPath);
    qputenv("PYTHONHOME", pythonPath);
  }

  // Set environment variable to indicate that we are running inside the Tomviz
  // application vs Python command line. This can be used to selectively load
  // modules.
  qputenv("TOMVIZ_APPLICATION", "1");

  setlocale(LC_NUMERIC, "C");
  pqPVApplicationCore appCore(argc, argv);
  tomviz::MainWindow window;
  window.show();
  splash.finish(&window);
  window.openFiles(argc, argv);

  return app.exec();
}

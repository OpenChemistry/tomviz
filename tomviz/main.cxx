/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>

#include <QSplashScreen>
#include <QSurfaceFormat>

#include <QDebug>

#include <pqPVApplicationCore.h>

#include <QVTKOpenGLStereoWidget.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>

#include "loguru.hpp"
#include "MainWindow.h"
#include "PythonUtilities.h"
#include "tomvizConfig.h"
#include "tomvizPythonConfig.h"

#include <clocale>

int main(int argc, char** argv)
{
  // Set up loguru, for printing stack traces on crashes
  loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
  loguru::init(argc, argv);

  QSurfaceFormat::setDefaultFormat(QVTKOpenGLStereoWidget::defaultFormat());

  QCoreApplication::setApplicationName("tomviz");
  QCoreApplication::setApplicationVersion(TOMVIZ_VERSION);
  QCoreApplication::setOrganizationName("tomviz");

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

  // If we don't initialize Python here, the application freezes when exiting
  // at the very end, during Py_Finalize().
  tomviz::Python::initialize();

  setlocale(LC_NUMERIC, "C");
  pqPVApplicationCore appCore(argc, argv);
  tomviz::MainWindow window;
  window.show();
  splash.finish(&window);
  window.openFiles(argc, argv);

  return app.exec();
}

// License?
#include <QApplication>

#ifdef Q_WS_X11
#include <QPlastiqueStyle>
#endif

#include "pqPVApplicationCore.h"
#include "MainWindow.h"

#include <clocale>

int main(int argc, char** argv)
{
#ifdef Q_WS_X11
  // Using motif style gives us test failures (and its ugly).
  // Using cleanlooks style gives us errors when using valgrind (Trolltech's bug #179200)
  // let's just use plastique for now
  QApplication::setStyle(new QPlastiqueStyle);
#endif

  QCoreApplication::setApplicationName("TEMTomography");
  QCoreApplication::setApplicationVersion("0.0.1");
  QCoreApplication::setOrganizationName("Kitware");

  QApplication app(argc, argv);
  setlocale(LC_NUMERIC,"C");
  pqPVApplicationCore appCore(argc, argv);
  TEM::MainWindow window;
  window.show();
  return app.exec();
}

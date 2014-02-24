// License?

#include <QMainWindow>

namespace TEM
{

  class MainWindow : public QMainWindow
  {
  Q_OBJECT;
  typedef QMainWindow Superclass;
public:
  MainWindow(QWidget* parent=0, Qt::WindowFlags flags=0);
  virtual ~MainWindow();

private:
  Q_DISABLE_COPY(MainWindow);
  class MWInternals;
  MWInternals* Internals;
  };
}

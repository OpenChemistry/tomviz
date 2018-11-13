/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataTransformMenu_h
#define tomvizDataTransformMenu_h

#include <QObject>

#include <QPointer>
#include <QScopedPointer>

class QMainWindow;
class QMenu;

namespace tomviz {

// DataTransformMenu is the manager for the Data Transform menu.
// It is responsible for enabling and disabling Data Transforms based
// on properties of the DataSource.
class DataTransformMenu : public QObject
{
  Q_OBJECT

public:
  DataTransformMenu(QMainWindow* mainWindow, QMenu* transform, QMenu* seg);

private slots:
  void updateActions();

protected slots:
  void buildTransforms();
  void buildSegmentation();

private:
  Q_DISABLE_COPY(DataTransformMenu)

  QMenu* m_transformMenu;
  QMenu* m_segmentationMenu;
  QMainWindow* m_mainWindow;
};
} // namespace tomviz

#endif

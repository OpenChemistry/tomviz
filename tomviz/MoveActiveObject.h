/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMoveActiveObject_h
#define tomvizMoveActiveObject_h

#include <QObject>

#include <QPoint>
#include <QPointer>

#include <vtkNew.h>
#include <vtkVector.h>

class pqDataRepresentation;
class vtkBoxRepresentation;
class vtkBoxWidget2;
class vtkEventQtSlotConnect;
class vtkObject;
class vtkSMViewProxy;
class pqView;

namespace tomviz {
class DataSource;

class MoveActiveObject : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  MoveActiveObject(QObject* parent);
  ~MoveActiveObject();

private slots:
  void dataSourceActivated(DataSource* ds);

  void updateForNewDataSource(DataSource* newDS);
  void hideMoveObjectWidget();
  void onViewChanged(vtkSMViewProxy* newView);
  void interactionEnd(vtkObject* obj);
  void setMoveEnabled(bool enable);

private:
  Q_DISABLE_COPY(MoveActiveObject)
  vtkNew<vtkBoxRepresentation> BoxRep;
  vtkNew<vtkBoxWidget2> BoxWidget;
  vtkNew<vtkEventQtSlotConnect> EventLink;
  QPointer<pqView> View;
  vtkVector3d DataLocation;
  bool MoveEnabled;
};
} // namespace tomviz

#endif

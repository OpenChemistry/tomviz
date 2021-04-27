/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizVtkTriangleBar_h
#define tomvizVtkTriangleBar_h

#include <vtkActor2D.h>

#include <vtkNew.h>

enum class Position
{
  Start = 0,
  Middle = 1,
  End = 2,
};

class vtkPoints;
class vtkCellArray;
class vtkPolyData;
class vtkPolyDataMapper2D;
class vtkPropCollection;
class vtkViewport;
class vtkWindow;
class vtkTextActor;

class vtkTriangleBar : public vtkActor2D
{
public:
  vtkTypeMacro(vtkTriangleBar, vtkActor2D) static vtkTriangleBar* New();

  void SetColors(double color0[3], double color1[3], double color2[3]);
  void SetLabels(const char* label0, const char* label1, const char* label2);
  void SetAlignment(Position horizontalPos, Position verticalPos);

  void GetActors(vtkPropCollection* pc) override;
  void GetActors2D(vtkPropCollection* pc) override;
  void ReleaseGraphicsResources(vtkWindow* w) override;
  int RenderOpaqueGeometry(vtkViewport* v) override;
  int RenderTranslucentPolygonalGeometry(vtkViewport* v) override;
  int RenderOverlay(vtkViewport* v) override;
  vtkTypeBool HasTranslucentPolygonalGeometry() override;

protected:
  vtkTriangleBar();
  ~vtkTriangleBar() override;

  void updateRepresentation(vtkViewport* v);

  vtkNew<vtkPoints> Points;
  vtkNew<vtkCellArray> Cells;
  vtkNew<vtkPolyData> Poly;
  vtkNew<vtkPolyDataMapper2D> Mapper;
  vtkNew<vtkActor2D> BarActor;
  vtkNew<vtkTextActor> LabelActor0;
  vtkNew<vtkTextActor> LabelActor1;
  vtkNew<vtkTextActor> LabelActor2;
  Position HorizontalPos;
  Position VerticalPos;
  vtkTimeStamp updateTime;
  int Edge;

private:
  vtkTriangleBar(const vtkTriangleBar&) = delete;
  void operator=(const vtkTriangleBar&) = delete;
};

#endif

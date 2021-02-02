/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

/**
 * @class   vtkLengthScaleRepresentation
 * @brief   represent the vtkDistanceWidget
 *
 * The vtkLengthScaleRepresentation is a representation for the
 * vtkDistanceWidget. This representation consists of a measuring line (axis)
 * and two vtkHandleWidgets to place the end points of the line. The length of
 * the line adapts to fit within a minimum and maximum fraction of the width of
 * the viewing plane.
 *
 * @sa
 * vtkDistanceWidget2D
*/

#ifndef vtkLengthScaleRepresentation_h
#define vtkLengthScaleRepresentation_h

#include "vtkDistanceRepresentation2D.h"

class vtkTextActor;

class vtkLengthScaleRepresentation : public vtkDistanceRepresentation2D
{
public:
  /**
   * Instantiate class.
   */
  static vtkLengthScaleRepresentation* New();

  /**
   * Method to satisfy superclasses' API.
   */
  void BuildRepresentation() override;

  //@{
  /**
   * Standard VTK methods.
   */
  vtkTypeMacro(
    vtkLengthScaleRepresentation,
    vtkDistanceRepresentation2D) void PrintSelf(ostream& os,
                                                vtkIndent indent) override;
  //@}

  //@{
  /**
   * Get the label actor
   */
  vtkGetObjectMacro(Label, vtkTextActor);
  //@}

  //@{
  /**
   * Set the visibility of both the cube and the label
   */
  void SetRepresentationVisibility(bool choice);
  //@}

  //@{
  /**
   * Set the length (default is 1).
   */
  void SetLength(double);
  vtkGetMacro(Length, double);
  //@}

  //@{
  /**
   * Set the label for the unit of length of a side of the cube.
   */
  vtkSetStringMacro(LengthUnit);
  vtkGetStringMacro(LengthUnit);
  //@}

  //@{
  /**
   * Set/Get the rescaling increment for the ruler length.
   */
  vtkSetClampMacro(RescaleFactor, double, 1., VTK_DOUBLE_MAX);
  vtkGetMacro(RescaleFactor, double);
  //@}

  //@{
  /**
   * Set the min/max representational length relative to the render window
   * width.
   */
  void SetMinRelativeScreenWidth(double);
  vtkGetMacro(MinRelativeScreenWidth, double);
  void SetMaxRelativeScreenWidth(double);
  vtkGetMacro(MaxRelativeScreenWidth, double);
  //@}

  //@{
  /**
   * Methods to make this class properly act like a vtkWidgetRepresentation.
   */
  void GetActors2D(vtkPropCollection*) override;
  void ReleaseGraphicsResources(vtkWindow*) override;
  int RenderOverlay(vtkViewport*) override;
  int RenderOpaqueGeometry(vtkViewport*) override;
  int RenderTranslucentPolygonalGeometry(vtkViewport*) override;
  int HasTranslucentPolygonalGeometry() override;
  //@}

protected:
  vtkLengthScaleRepresentation();
  ~vtkLengthScaleRepresentation() override;

  void UpdateRuler();
  void UpdateLabel();
  void ScaleIfNecessary(vtkViewport*);

  vtkTextActor* Label;
  double RescaleFactor;
  double MinRelativeScreenWidth;
  double MaxRelativeScreenWidth;
  double Length;
  char* LengthUnit;

private:
  vtkLengthScaleRepresentation(const vtkLengthScaleRepresentation&) = delete;
  void operator=(const vtkLengthScaleRepresentation&) = delete;
};

#endif

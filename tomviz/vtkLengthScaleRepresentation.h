/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkLengthScaleRepresentation.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
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
  void BuildRepresentation() VTK_OVERRIDE;

  //@{
  /**
   * Standard VTK methods.
   */
  vtkTypeMacro(vtkLengthScaleRepresentation,
               vtkDistanceRepresentation2D) void PrintSelf(ostream& os,
                                                           vtkIndent indent)
    VTK_OVERRIDE;
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
  void GetActors2D(vtkPropCollection*) VTK_OVERRIDE;
  void ReleaseGraphicsResources(vtkWindow*) VTK_OVERRIDE;
  int RenderOverlay(vtkViewport*) VTK_OVERRIDE;
  int RenderOpaqueGeometry(vtkViewport*) VTK_OVERRIDE;
  int RenderTranslucentPolygonalGeometry(vtkViewport*) VTK_OVERRIDE;
  int HasTranslucentPolygonalGeometry() VTK_OVERRIDE;
  //@}

protected:
  vtkLengthScaleRepresentation();
  ~vtkLengthScaleRepresentation() VTK_OVERRIDE;

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
  vtkLengthScaleRepresentation(const vtkLengthScaleRepresentation&)
    VTK_DELETE_FUNCTION;
  void operator=(const vtkLengthScaleRepresentation&) VTK_DELETE_FUNCTION;
};

#endif

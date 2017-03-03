/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkVolumeScaleRepresentation.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/**
 * @class   vtkVolumeScaleRepresentation
 * @brief   represent the vtkDistanceWidget
 *
 * The vtkVolumeScaleRepresentation is a representation for the
 * vtkHandleWidget. This representation consists of a measuring cube. The size
 * of the cube adapts to fit within a minimum and maximum fraction of the
 * viewing area.
 *
 * @sa
 * vtkMeasurementCubeHandleRepresentation3D
*/

#ifndef vtkVolumeScaleRepresentation_h
#define vtkVolumeScaleRepresentation_h

#include "vtkMeasurementCubeHandleRepresentation3D.h"

class vtkTextActor;

class vtkVolumeScaleRepresentation :
  public vtkMeasurementCubeHandleRepresentation3D
{
public:
  /**
   * Instantiate class.
   */
  static vtkVolumeScaleRepresentation *New();

  /**
   * Method to satisfy superclasses' API.
   */
  void BuildRepresentation() VTK_OVERRIDE;

  //@{
  /**
   * Standard VTK methods.
   */
  vtkTypeMacro(vtkVolumeScaleRepresentation,
               vtkMeasurementCubeHandleRepresentation3D)
  void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;
  //@}

  //@{
  /**
   * Get the label actor
   */
  vtkGetObjectMacro( Label, vtkTextActor );
  //@}

  //@{
  /**
   * Set the visibility of both the cube and the label
   */
  void SetRepresentationVisibility(bool choice);
  //@}

  //@{
  /**
   * Methods to make this class properly act like a vtkWidgetRepresentation.
   */
  void GetActors2D(vtkPropCollection *) VTK_OVERRIDE;
  void ReleaseGraphicsResources(vtkWindow *) VTK_OVERRIDE;
  int RenderOverlay(vtkViewport *) VTK_OVERRIDE;
  int RenderOpaqueGeometry(vtkViewport *) VTK_OVERRIDE;
  int RenderTranslucentPolygonalGeometry(vtkViewport *) VTK_OVERRIDE;
  int HasTranslucentPolygonalGeometry() VTK_OVERRIDE;
  //@}

protected:
  vtkVolumeScaleRepresentation();
  ~vtkVolumeScaleRepresentation() VTK_OVERRIDE;

  void Update2DLabel();

  vtkTextActor* Label;

private:
  vtkVolumeScaleRepresentation(const vtkVolumeScaleRepresentation&) VTK_DELETE_FUNCTION;
  void operator=(const vtkVolumeScaleRepresentation&) VTK_DELETE_FUNCTION;
};

#endif

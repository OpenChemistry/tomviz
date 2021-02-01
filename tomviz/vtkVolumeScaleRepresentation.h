/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

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

class vtkVolumeScaleRepresentation
  : public vtkMeasurementCubeHandleRepresentation3D
{
public:
  /**
   * Instantiate class.
   */
  static vtkVolumeScaleRepresentation* New();

  /**
   * Method to satisfy superclasses' API.
   */
  void BuildRepresentation() override;

  //@{
  /**
   * Standard VTK methods.
   */
  vtkTypeMacro(
    vtkVolumeScaleRepresentation,
    vtkMeasurementCubeHandleRepresentation3D) void PrintSelf(ostream& os,
                                                             vtkIndent indent)
    override;
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
  vtkVolumeScaleRepresentation();
  ~vtkVolumeScaleRepresentation() override;

  void Update2DLabel();

  vtkTextActor* Label;

private:
  vtkVolumeScaleRepresentation(const vtkVolumeScaleRepresentation&)
    = delete;
  void operator=(const vtkVolumeScaleRepresentation&) = delete;
};

#endif

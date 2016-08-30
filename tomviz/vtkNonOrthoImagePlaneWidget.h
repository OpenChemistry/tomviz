/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkNonOrthoImagePlaneWidget.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkNonOrthoImagePlaneWidget - 3D widget for reslicing image data
// .SECTION Description
// This 3D widget defines a plane that can be interactively placed in an
// image volume. A nice feature of the object is that the
// vtkNonOrthoImagePlaneWidget, like any 3D widget, will work with the current
// interactor style. That is, if vtkNonOrthoImagePlaneWidget does not handle an
// event, then all other registered observers (including the interactor
// style) have an opportunity to process the event. Otherwise, the
// vtkNonOrthoImagePlaneWidget will terminate the processing of the event that
// it handles.
//
// The core functionality of the widget is provided by a vtkImageReslice
// object which passes its output onto a texture mapping pipeline for fast
// slicing through volumetric data. See the key methods: GenerateTexturePlane()
// and UpdatePlane() for implementation details.
//
// To use this object, just invoke SetInteractor() with the argument of the
// method a vtkRenderWindowInteractor.  You may also wish to invoke
// "PlaceWidget()" to initially position the widget. If the "i" key (for
// "interactor") is pressed, the vtkNonOrthoImagePlaneWidget will appear. (See
// superclass documentation for information about changing this behavior.)
//
// Selecting the widget with the left or middle mouse button enables
// reslicing capablilites..
// To facilitate use, a set of 'margins' (left, right, top, bottom) are shown as
// a set of plane-axes aligned lines, the properties of which can be changed
// as a group.
// Without keyboard modifiers: selecting in the middle of the margins
// enables translation of the plane along its normal. Selecting within a margin
// allows rotating about the center of the plane around an axis aligned with the
// margin (i.e., selecting left margin enables rotating around the plane's
// local y-prime axis).
//
// Events that occur outside of the widget (i.e., no part of the widget is
// picked) are propagated to any other registered obsevers (such as the
// interaction style). Turn off the widget by pressing the "i" key again
// (or invoke the Off() method). To support interactive manipulation of
// objects, this class invokes the events StartInteractionEvent,
// InteractionEvent, and EndInteractionEvent as well as StartWindowLevelEvent,
// WindowLevelEvent, EndWindowLevelEvent and ResetWindowLevelEvent.
//
// The vtkNonOrthoImagePlaneWidget has several methods that can be used in
// conjunction with other VTK objects. The GetPolyData() method can be used
// to get the polygonal representation of the plane and can be used as input
// for other VTK objects. Typical usage of the widget is to make use of the
// StartInteractionEvent, InteractionEvent, and EndInteractionEvent
// events. The InteractionEvent is called on mouse motion; the other two
// events are called on button down and button up (either left or right
// button).
//
// Some additional features of this class include the ability to control the
// properties of the widget. You can set the properties of: the selected and
// unselected representations of the plane's outline; the text actor via its
// vtkTextProperty; the cross-hair cursor. In addition there are methods to
// constrain the plane so that it is aligned along the x-y-z axes.  Finally,
// one can specify the degree of interpolation (vtkImageReslice): nearest
// neighbour, linear, and cubic.

// .SECTION Thanks
// Thanks to Dean Inglis for developing and contributing this class.
// Based on the Python SlicePlaneFactory from Atamai, Inc.

// .SECTION See Also
// vtk3DWidget vtkBoxWidget vtkLineWidget  vtkPlaneWidget vtkPointWidget
// vtkPolyDataSourceWidget vtkSphereWidget vtkImplicitPlaneWidget

#ifndef tomvizvtkNonOrthoImagePlaneWidget_h
#define tomvizvtkNonOrthoImagePlaneWidget_h

#include "vtkPolyDataSourceWidget.h"

class vtkAbstractPropPicker;
class vtkActor;
class vtkConeSource;
class vtkDataSetMapper;
class vtkImageData;
class vtkImageMapToColors;
class vtkImageReslice;
class vtkInformation;
class vtkLineSource;
class vtkLookupTable;
class vtkMatrix4x4;
class vtkPlaneSource;
class vtkPoints;
class vtkPolyData;
class vtkProperty;
class vtkScalarsToColors;
class vtkSphereSource;
class vtkTextActor;
class vtkTextProperty;
class vtkTexture;
class vtkTransform;

#define VTK_NEAREST_RESLICE 0
#define VTK_LINEAR_RESLICE 1
#define VTK_CUBIC_RESLICE 2

class vtkNonOrthoImagePlaneWidget : public vtkPolyDataSourceWidget
{
public:
  // Description:
  // Instantiate the object.
  static vtkNonOrthoImagePlaneWidget* New();

  vtkTypeMacro(
    vtkNonOrthoImagePlaneWidget,
    vtkPolyDataSourceWidget) void PrintSelf(ostream& os,
                                            vtkIndent indent) override;

  // Description:
  // Methods that satisfy the superclass' API.
  void SetEnabled(int) override;
  void PlaceWidget(double bounds[6]) override;
  void PlaceWidget() override { this->Superclass::PlaceWidget(); }
  void PlaceWidget(double xmin, double xmax, double ymin, double ymax,
                   double zmin, double zmax) override
  {
    this->Superclass::PlaceWidget(xmin, xmax, ymin, ymax, zmin, zmax);
  }

  // Description:
  // Set the vtkImageData* input for the vtkImageReslice.
  void SetInputConnection(vtkAlgorithmOutput* aout) override;

  // Description:
  // Set/Get the origin of the plane.  Set origin changes the size of the plane
  // by moving the origin and leaving the other two points fixed.
  void SetOrigin(double x, double y, double z);
  void SetOrigin(double xyz[3]);
  double* GetOrigin();
  void GetOrigin(double xyz[3]);

  // Description:
  // Set/Get the position of the point defining the first axis of the plane.
  void SetPoint1(double x, double y, double z);
  void SetPoint1(double xyz[3]);
  double* GetPoint1();
  void GetPoint1(double xyz[3]);

  // Description:
  // Set/Get the position of the point defining the second axis of the plane.
  void SetPoint2(double x, double y, double z);
  void SetPoint2(double xyz[3]);
  double* GetPoint2();
  void GetPoint2(double xyz[3]);

  // Description:
  // Set/Get the center of the plane.  SetCenter translates the plane by the
  // difference between the old and new center positions.
  void SetCenter(double xyz[3]);
  double* GetCenter();
  void GetCenter(double xyz[3]);

  // Description:
  // Set/Get the normal to the plane.  SetNormal rotates the plane about its
  // center.
  void SetNormal(double xyz[3]);
  double* GetNormal();
  void GetNormal(double xyz[3]);

  // Description:
  // Set/Get the diplay offset.  This translates the entire widget by the vector
  // given.
  void SetDisplayOffset(const double xyz[3]);
  const double* GetDisplayOffset();
  void GetDisplayOffset(double xyz[3]);

  // Description:
  // Get the vector from the plane origin to point1.
  void GetVector1(double v1[3]);

  // Description:
  // Get the vector from the plane origin to point2.
  void GetVector2(double v2[3]);

  // Description:
  // Get the slice position in terms of the data extent.
  int GetSliceIndex();

  // Description:
  // Set the slice position in terms of the data extent.
  void SetSliceIndex(int index);

  // Description:
  // Get the position of the slice along its normal.
  double GetSlicePosition();

  // Description:
  // Set the position of the slice along its normal.
  void SetSlicePosition(double position);

  // Description:
  // Set the interpolation to use when texturing the plane.
  void SetResliceInterpolate(int);
  vtkGetMacro(ResliceInterpolate, int)
  void SetResliceInterpolateToNearestNeighbour()
  {
    this->SetResliceInterpolate(VTK_NEAREST_RESLICE);
  }
  void SetResliceInterpolateToLinear()
  {
    this->SetResliceInterpolate(VTK_LINEAR_RESLICE);
  }
  void SetResliceInterpolateToCubic()
  {
    this->SetResliceInterpolate(VTK_CUBIC_RESLICE);
  }

  // Description:
  // Convenience method to get the vtkImageReslice output.
  vtkImageData* GetResliceOutput();

  // Description:
  // Specify whether to interpolate the texture or not. When off, the
  // reslice interpolation is nearest neighbour regardless of how the
  // interpolation is set through the API. Set before setting the
  // vtkImageData input. Default is On.
  vtkSetMacro(TextureInterpolate, int)
  vtkGetMacro(TextureInterpolate, int)
  vtkBooleanMacro(TextureInterpolate, int)

  // Description:
  // Control the visibility of the actual texture mapped reformatted plane.
  // in some cases you may only want the plane outline for example.
  virtual void SetTextureVisibility(int);
  vtkGetMacro(TextureVisibility, int)
  vtkBooleanMacro(TextureVisibility, int)

  // Description:
  // Grab the polydata (including points) that defines the plane.  The
  // polydata consists of (res+1)*(res+1) points, and res*res quadrilateral
  // polygons, where res is the resolution of the plane. These point values
  // are guaranteed to be up-to-date when either the InteractionEvent or
  // EndInteraction events are invoked. The user provides the vtkPolyData and
  // the points and polygons are added to it.
  void GetPolyData(vtkPolyData* pd);

  // Description:
  // Satisfies superclass API.  This returns a pointer to the underlying
  // vtkPolyData.  Make changes to this before calling the initial PlaceWidget()
  // to have the initial placement follow suit.  Or, make changes after the
  // widget has been initialised and call UpdatePlacement() to realise.
  vtkPolyDataAlgorithm* GetPolyDataAlgorithm() override;

  // Description:
  // Satisfies superclass API.  This will change the state of the widget to
  // match changes that have been made to the underlying vtkPolyDataSource
  void UpdatePlacement(void) override;

  // Description:
  // Convenience method to get the texture used by this widget.  This can be
  // used in external slice viewers.
  vtkTexture* GetTexture();

  // Description:
  // Set/Get the plane's outline properties. The properties of the plane's
  // outline when selected and unselected can be manipulated.
  virtual void SetPlaneProperty(vtkProperty*);
  vtkGetObjectMacro(PlaneProperty, vtkProperty)
  virtual void SetSelectedPlaneProperty(vtkProperty*);
  vtkGetObjectMacro(SelectedPlaneProperty, vtkProperty)

  // Description:
  // Set/Get the arrows's outline properties. The properties of the arrow's
  // outline when selected and unselected can be manipulated.
  virtual void SetArrowProperty(vtkProperty*);
  vtkGetObjectMacro(ArrowProperty, vtkProperty)
  virtual void SetSelectedArrowProperty(vtkProperty*);
  vtkGetObjectMacro(SelectedArrowProperty, vtkProperty)

  // Description:
  // Convenience method sets the plane orientation normal to the
  // x, y, or z axes.  Default is XAxes (0).
  void SetPlaneOrientation(int);
  vtkGetMacro(PlaneOrientation, int)
  void SetPlaneOrientationToXAxes() { this->SetPlaneOrientation(0); }
  void SetPlaneOrientationToYAxes() { this->SetPlaneOrientation(1); }
  void SetPlaneOrientationToZAxes() { this->SetPlaneOrientation(2); }

  // Description:
  // Set the internal picker to one defined by the user.  In this way,
  // a set of three orthogonal planes can share the same picker so that
  // picking is performed correctly.  The default internal picker can be
  // re-set/allocated by setting to 0 (NULL).
  void SetPicker(vtkAbstractPropPicker*);

  // Description:
  // Set/Get the internal lookuptable (lut) to one defined by the user, or,
  // alternatively, to the lut of another vtkImgePlaneWidget.  The default
  // internal lut can be re- set/allocated by setting to 0 (NULL).
  virtual void SetLookupTable(vtkScalarsToColors*);
  vtkGetObjectMacro(LookupTable, vtkScalarsToColors)

  // Description:
  // Set/Get the property for the resliced image.
  virtual void SetTexturePlaneProperty(vtkProperty*);
  vtkGetObjectMacro(TexturePlaneProperty, vtkProperty)

  // Description:
  // Get the current reslice class and reslice axes
  vtkGetObjectMacro(ResliceAxes, vtkMatrix4x4)
  vtkGetObjectMacro(Reslice, vtkImageReslice)

  // Description:
  // Enable/disable mouse interaction so the widget remains on display.
  void SetInteraction(int interact);
  vtkGetMacro(Interaction, int);
  vtkBooleanMacro(Interaction, int);

  // Description:
  // Set the arrow visible or invisible so only the plane remains on display.
  // This disables interaction with the arrow since only visible actors are
  // pickable, but leaves interaction with the plane up to the state of
  // SetInteraction.
  void SetArrowVisibility(int visible);
  vtkGetMacro(ArrowVisibility, int);
  vtkBooleanMacro(ArrowVisibility, int);

  // BTX
  // Description:
  // Set action associated to buttons.
  enum
  {
    VTK_NO_ACTION = 0,
    VTK_SLICE_MOTION_ACTION = 1
  };
  // ETX
  vtkSetClampMacro(LeftButtonAction, int, VTK_NO_ACTION,
                   VTK_SLICE_MOTION_ACTION)
  vtkGetMacro(LeftButtonAction, int)
  vtkSetClampMacro(MiddleButtonAction, int, VTK_NO_ACTION,
                   VTK_SLICE_MOTION_ACTION)
  vtkGetMacro(MiddleButtonAction, int)
  vtkSetClampMacro(RightButtonAction, int, VTK_NO_ACTION,
                   VTK_SLICE_MOTION_ACTION)
  vtkGetMacro(RightButtonAction, int)

protected:
  vtkNonOrthoImagePlaneWidget();
  ~vtkNonOrthoImagePlaneWidget();

  int TextureVisibility;

  int LeftButtonAction;
  int MiddleButtonAction;
  int RightButtonAction;

  // BTX
  enum
  {
    VTK_NO_BUTTON = 0,
    VTK_LEFT_BUTTON = 1,
    VTK_MIDDLE_BUTTON = 2,
    VTK_RIGHT_BUTTON = 3
  };
  // ETX
  int LastButtonPressed;

  // BTX - manage the state of the widget
  int State;
  enum WidgetState
  {
    Start = 0,
    Pushing,
    Rotating,
    Outside
  };
  // ETX

  // Handles the events
  static void ProcessEvents(vtkObject* object, unsigned long event,
                            void* clientdata, void* calldata);

  // internal utility method that adds observers to the RenderWindowInteractor
  // so that our ProcessEvents is eventually called.  this method is called
  // by SetEnabled as well as SetInteraction
  void AddObservers();

  // ProcessEvents() dispatches to these methods.
  virtual void OnMouseMove();
  void OnLeftButtonDown() { this->OnButtonDown(&this->LeftButtonAction); }
  void OnLeftButtonUp() { this->OnButtonUp(&this->LeftButtonAction); }
  void OnMiddleButtonDown() { this->OnButtonDown(&this->MiddleButtonAction); }
  void OnMiddleButtonUp() { this->OnButtonUp(&this->MiddleButtonAction); }
  void OnRightButtonDown() { this->OnButtonDown(&this->RightButtonAction); }
  void OnRightButtonUp() { this->OnButtonUp(&this->RightButtonAction); }

  virtual void OnButtonDown(int* btn);
  virtual void OnButtonUp(int* btn);

  virtual void StartSliceMotion();
  virtual void StopSliceMotion();

  // controlling ivars
  int Interaction; // Is the widget responsive to mouse events
  int ArrowVisibility;
  int PlaneOrientation;
  int ResliceInterpolate;
  int TextureInterpolate;

  // display offset
  double DisplayOffset[3];
  vtkTransform* DisplayTransform;

  // The geometric represenation of the plane and it's outline
  vtkPlaneSource* PlaneSource;
  vtkPolyData* PlaneOutlinePolyData;
  vtkActor* PlaneOutlineActor;
  void HighlightPlane(int highlight);
  void GeneratePlaneOutline();

  // Re-builds the plane outline based on the plane source
  void BuildRepresentation();

  // Do the picking
  vtkAbstractPropPicker* PlanePicker;
  // Register internal Pickers within PickingManager
  void RegisterPickers() override;

  // Methods to manipulate the plane
  void Push(double* p1, double* p2);
  void Rotate(double X, double Y, double* p1, double* p2, double* vpn);

  vtkImageData* ImageData;
  vtkImageReslice* Reslice;
  vtkMatrix4x4* ResliceAxes;
  vtkTransform* Transform;
  vtkActor* TexturePlaneActor;
  vtkTexture* Texture;
  vtkScalarsToColors* LookupTable;
  vtkScalarsToColors* CreateDefaultLookupTable();

  // Properties used to control the appearance of selected objects and
  // the manipulator in general.  The plane property is actually that for
  // the outline.  The TexturePlaneProperty can be used to control the
  // lighting etc. of the resliced image data.
  vtkProperty* PlaneProperty;         // used when not interacting
  vtkProperty* SelectedPlaneProperty; // used when interacting
  vtkProperty* ArrowProperty;
  vtkProperty* SelectedArrowProperty;
  vtkProperty* TexturePlaneProperty;
  void CreateDefaultProperties();

  // Reslice and texture management
  void UpdatePlane();
  void FindPlaneBounds(vtkInformation* outInfo, double bounds[6]);
  void UpdateClipBounds(double bounds[6], double spacing[3]);

  void GenerateTexturePlane();

  // The + normal cone
  vtkConeSource* ConeSource;
  vtkActor* ConeActor;

  // The + normal line
  vtkLineSource* LineSource;
  vtkActor* LineActor;

  // The - normal cone
  vtkConeSource* ConeSource2;
  vtkActor* ConeActor2;

  // The - normal line
  vtkLineSource* LineSource2;
  vtkActor* LineActor2;

  // The origin positioning handle
  vtkSphereSource* Sphere;
  vtkActor* SphereActor;

  void HighlightArrow(int highlight);
  void GenerateArrow(); // generate the default arrow
  void UpdateArrowSize(); // update the arrow to be visible based on camera pos

private:
  vtkNonOrthoImagePlaneWidget(
    const vtkNonOrthoImagePlaneWidget&);              // Not implemented
  void operator=(const vtkNonOrthoImagePlaneWidget&); // Not implemented
};

#endif

/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "vtkTransferFunctionBoxItem.h"

#include <vtkBrush.h>
#include <vtkColorTransferFunction.h>
#include <vtkContext2D.h>
#include <vtkContextMouseEvent.h>
#include <vtkContextScene.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPen.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkPoints2D.h>
#include <vtkTransform2D.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVectorOperators.h>

#include "vtkTransferFunction2DItem.h"

namespace {
inline bool PointIsWithinBounds2D(double point[2], double bounds[4],
                                  const double delta[2])
{
  if (!point || !bounds || !delta) {
    return false;
  }

  for (int i = 0; i < 2; i++) {
    if (point[i] + delta[i] < bounds[2 * i] ||
        point[i] - delta[i] > bounds[2 * i + 1]) {
      return false;
    }
  }
  return true;
}
}

////////////////////////////////////////////////////////////////////////////////
vtkStandardNewMacro(vtkTransferFunctionBoxItem)

  vtkTransferFunctionBoxItem::vtkTransferFunctionBoxItem()
  : Superclass()
{
  this->ObserverNum = 0;
  // Initialize box, points are ordered as:
  //     3 ----- 2
  //     |       |
  // (4) 0 ----- 1
  this->AddPoint(1.0, 1.0);
  this->AddPoint(20.0, 1.0);
  this->AddPoint(20.0, 20.0);
  this->AddPoint(1.0, 20.0);

  // Point 0 is repeated for rendering purposes
  this->BoxPoints->InsertNextPoint(1.0, 1.0);

  // Initialize outline
  this->Pen->SetWidth(2.);
  this->Pen->SetColor(255, 255, 255);
  this->Pen->SetLineType(vtkPen::SOLID_LINE);

  // Initialize texture
  auto tex = this->Texture.GetPointer();
  const int texSize = 256;
  tex->SetDimensions(texSize, 1, 1);
  tex->AllocateScalars(VTK_UNSIGNED_CHAR, 4);
  auto arr =
    vtkUnsignedCharArray::SafeDownCast(tex->GetPointData()->GetScalars());
  const auto dataPtr = arr->GetVoidPointer(0);
  memset(dataPtr, 0, texSize * 4 * sizeof(unsigned char));
  this->IsUpdatingBox = false;
}

vtkTransferFunctionBoxItem::~vtkTransferFunctionBoxItem() = default;

void vtkTransferFunctionBoxItem::SetItem(vtkTransferFunction2DItem* item)
{
  if (this->TransferFunctionItem != item) {
    if (this->TransferFunctionItem) {
      this->TransferFunctionItem->RemoveObserver(this->ObserverNum);
    }
    this->TransferFunctionItem = item;
    // Capture changes to the box
    if (this->TransferFunctionItem) {
      this->TransferFunctionItem->AddObserver(vtkCommand::ModifiedEvent, this,
                                              &vtkObject::Modified);
      vtkRectd newBox = this->TransferFunctionItem->GetBox();
      this->SetBox(newBox.GetX(), newBox.GetY(), newBox.GetWidth(),
                   newBox.GetHeight());
    }
    this->Modified();
  }
}

vtkTransferFunction2DItem* vtkTransferFunctionBoxItem::GetItem()
{
  return this->TransferFunctionItem;
}

void vtkTransferFunctionBoxItem::DragBox(const double deltaX,
                                         const double deltaY)
{
  this->StartChanges();

  if (!BoxIsWithinBounds(deltaX, deltaY))
    return;

  this->MovePoint(BOTTOM_LEFT, deltaX, deltaY);
  this->MovePoint(BOTTOM_LEFT_LOOP, deltaX, deltaY);
  this->MovePoint(BOTTOM_RIGHT, deltaX, deltaY);
  this->MovePoint(TOP_RIGHT, deltaX, deltaY);
  this->MovePoint(TOP_LEFT, deltaX, deltaY);

  this->UpdateInternalBox();
  this->EndChanges();
  this->InvokeEvent(vtkCommand::SelectionChangedEvent);
}

bool vtkTransferFunctionBoxItem::BoxIsWithinBounds(const double deltaX,
                                                   const double deltaY)
{
  double bounds[4];
  this->GetValidBounds(bounds);

  const double delta[2] = { 0.0, 0.0 };
  for (vtkIdType id = 0; id < this->NumPoints; id++) {
    double pos[2];
    this->BoxPoints->GetPoint(id, pos);
    pos[0] += deltaX;
    pos[1] += deltaY;
    if (!PointIsWithinBounds2D(pos, bounds, delta))
      return false;
  }
  return true;
}

void vtkTransferFunctionBoxItem::MovePoint(vtkIdType pointId,
                                           const double deltaX,
                                           const double deltaY)
{
  double pos[2];
  this->BoxPoints->GetPoint(pointId, pos);

  double newPos[2] = { pos[0] + deltaX, pos[1] + deltaY };
  this->ClampToValidPosition(newPos);

  this->BoxPoints->SetPoint(pointId, newPos[0], newPos[1]);
}

vtkIdType vtkTransferFunctionBoxItem::AddPoint(const double x, const double y)
{
  double pos[2] = { x, y };
  return this->AddPoint(pos);
}

vtkIdType vtkTransferFunctionBoxItem::AddPoint(double* pos)
{
  if (this->BoxPoints->GetNumberOfPoints() >= 4) {
    return 3;
  }

  this->StartChanges();

  const vtkIdType id = this->BoxPoints->InsertNextPoint(pos[0], pos[1]);
  Superclass::AddPointId(id);

  this->UpdateInternalBox();
  this->EndChanges();

  return id;
}

void vtkTransferFunctionBoxItem::DragCorner(const vtkIdType cornerId,
                                            const double* delta)
{
  if (cornerId < 0 || cornerId > 3) {
    return;
  }

  this->StartChanges();

  // Move dragged corner and adjacent corners
  switch (cornerId) {
    case BOTTOM_LEFT:
      if (this->ArePointsCrossing(cornerId, delta, TOP_RIGHT))
        break;
      this->MovePoint(cornerId, delta[0], delta[1]);
      this->MovePoint(BOTTOM_LEFT_LOOP, delta[0], delta[1]);
      this->MovePoint(TOP_LEFT, delta[0], 0.0);
      this->MovePoint(BOTTOM_RIGHT, 0.0, delta[1]);
      break;

    case BOTTOM_RIGHT:
      if (this->ArePointsCrossing(cornerId, delta, TOP_LEFT))
        break;
      this->MovePoint(cornerId, delta[0], delta[1]);
      this->MovePoint(BOTTOM_LEFT, 0.0, delta[1]);
      this->MovePoint(BOTTOM_LEFT_LOOP, 0.0, delta[1]);
      this->MovePoint(TOP_RIGHT, delta[0], 0.0);
      break;

    case TOP_RIGHT:
      if (this->ArePointsCrossing(cornerId, delta, BOTTOM_LEFT))
        break;
      this->MovePoint(cornerId, delta[0], delta[1]);
      this->MovePoint(BOTTOM_RIGHT, delta[0], 0.0);
      this->MovePoint(TOP_LEFT, 0.0, delta[1]);
      break;

    case TOP_LEFT:
      if (this->ArePointsCrossing(cornerId, delta, BOTTOM_RIGHT))
        break;
      this->MovePoint(cornerId, delta[0], delta[1]);
      this->MovePoint(TOP_RIGHT, 0.0, delta[1]);
      this->MovePoint(BOTTOM_LEFT, delta[0], 0.0);
      this->MovePoint(BOTTOM_LEFT_LOOP, delta[0], 0.0);
      break;
  }

  this->UpdateInternalBox();
  this->EndChanges();
  this->InvokeEvent(vtkCommand::SelectionChangedEvent);
}

bool vtkTransferFunctionBoxItem::ArePointsCrossing(const vtkIdType pointA,
                                                   const double* deltaA,
                                                   const vtkIdType pointB)
{
  double posA[2];
  this->BoxPoints->GetPoint(pointA, posA);

  double posB[2];
  this->BoxPoints->GetPoint(pointB, posB);

  const double distXBefore = posA[0] - posB[0];
  const double distXAfter = posA[0] + deltaA[0] - posB[0];
  if (distXAfter * distXBefore <= 0.0) // Sign changed
  {
    return true;
  }

  const double distYBefore = posA[1] - posB[1];
  const double distYAfter = posA[1] + deltaA[1] - posB[1];
  if (distYAfter * distYBefore <= 0.0) // Sign changed
  {
    return true;
  }

  return false;
}

vtkIdType vtkTransferFunctionBoxItem::RemovePoint(double* vtkNotUsed(pos))
{
  // This method does nothing as this item has a fixed number of points (4).
  return 0;
}

void vtkTransferFunctionBoxItem::SetControlPoint(vtkIdType vtkNotUsed(index),
                                                 double* vtkNotUsed(point))
{
  // This method does nothing as this item has a fixed number of points (4).
  return;
}

vtkIdType vtkTransferFunctionBoxItem::GetNumberOfPoints() const
{
  return static_cast<vtkIdType>(this->NumPoints);
}

void vtkTransferFunctionBoxItem::GetControlPoint(vtkIdType index,
                                                 double* point) const
{
  if (index >= this->NumPoints) {
    return;
  }

  this->BoxPoints->GetPoint(index, point);
}

vtkMTimeType vtkTransferFunctionBoxItem::GetControlPointsMTime()
{
  return this->GetMTime();
}

void vtkTransferFunctionBoxItem::emitEvent(unsigned long event, void* params)
{
  this->InvokeEvent(event, params);
}

bool vtkTransferFunctionBoxItem::Paint(vtkContext2D* painter)
{
  // Prepare brush
  if (this->IsInitialized() && this->NeedsTextureUpdate()) {
    this->ComputeTexture();
  }

  auto brush = painter->GetBrush();
  brush->SetColorF(0.0, 0.0, 0.0, 0.0);
  brush->SetTexture(this->Texture.GetPointer());
  brush->SetTextureProperties(vtkBrush::Linear | vtkBrush::Stretch);

  // Prepare outline
  painter->ApplyPen(this->Pen.GetPointer());

  painter->DrawPolygon(this->BoxPoints.GetPointer());
  return Superclass::Paint(painter);
}

void vtkTransferFunctionBoxItem::ComputeTexture()
{
  auto ColorFunction = this->TransferFunctionItem->GetColorTransferFunction();

  double range[2];
  ColorFunction->GetRange(range);

  const int texSize = this->Texture->GetDimensions()[0];
  auto dataRGB = new double[texSize * 3];
  ColorFunction->GetTable(range[0], range[1], texSize, dataRGB);

  auto dataAlpha = new double[texSize];
  this->TransferFunctionItem->GetOpacityFunction()->GetTable(
    range[0], range[1], texSize, dataAlpha);

  auto arr = vtkUnsignedCharArray::SafeDownCast(
    this->Texture->GetPointData()->GetScalars());

  for (vtkIdType i = 0; i < texSize; i++) {

    double color[4];
    color[0] = dataRGB[i * 3] * 255.0;
    color[1] = dataRGB[i * 3 + 1] * 255.0;
    color[2] = dataRGB[i * 3 + 2] * 255.0;
    color[3] = dataAlpha[i] * 255.0;

    arr->SetTuple(i, color);
  }
  this->Texture->Modified();

  delete[] dataRGB;
  delete[] dataAlpha;
}

bool vtkTransferFunctionBoxItem::Hit(const vtkContextMouseEvent& mouse)
{
  vtkVector2f vpos = mouse.GetPos();
  this->TransformScreenToData(vpos, vpos);

  double pos[2];
  pos[0] = vpos.GetX();
  pos[1] = vpos.GetY();

  double bounds[4];
  this->GetBounds(bounds);

  const double delta[2] = { 0.0, 0.0 };
  const bool isWithinBox = PointIsWithinBounds2D(pos, bounds, delta);

  // maybe the cursor is over the first or last point (which could be outside
  // the bounds because of the screen point size).
  bool isOverPoint = false;
  for (int i = 0; i < this->NumPoints; ++i) {
    isOverPoint = this->IsOverPoint(pos, i);
    if (isOverPoint) {
      break;
    }
  }

  return isWithinBox || isOverPoint;
}

bool vtkTransferFunctionBoxItem::MouseButtonPressEvent(
  const vtkContextMouseEvent& mouse)
{
  this->MouseMoved = false;
  this->PointToToggle = -1;

  vtkVector2f vpos = mouse.GetPos();
  this->TransformScreenToData(vpos, vpos);
  double pos[2];
  pos[0] = vpos.GetX();
  pos[1] = vpos.GetY();
  vtkIdType pointUnderMouse = this->FindBoxPoint(pos);

  if (mouse.GetButton() == vtkContextMouseEvent::LEFT_BUTTON) {
    if (pointUnderMouse != -1) {
      this->SetCurrentPoint(pointUnderMouse);
      return true;
    } else {
      this->SetCurrentPoint(-1);
    }
    return true;
  }

  return false;
}

bool vtkTransferFunctionBoxItem::MouseButtonReleaseEvent(
  const vtkContextMouseEvent& mouse)
{
  return Superclass::MouseButtonReleaseEvent(mouse);
}

bool vtkTransferFunctionBoxItem::MouseDoubleClickEvent(
  const vtkContextMouseEvent& mouse)
{
  return Superclass::MouseDoubleClickEvent(mouse);
}

bool vtkTransferFunctionBoxItem::MouseMoveEvent(
  const vtkContextMouseEvent& mouse)
{
  switch (mouse.GetButton()) {
    case vtkContextMouseEvent::LEFT_BUTTON:
      if (this->CurrentPoint == -1) {
        // Drag box
        vtkVector2f deltaPos = mouse.GetPos() - mouse.GetLastPos();
        this->DragBox(deltaPos.GetX(), deltaPos.GetY());
        this->Scene->SetDirty(true);
        return true;
      } else {
        // Drag corner
        vtkVector2d deltaPos =
          (mouse.GetPos() - mouse.GetLastPos()).Cast<double>();
        this->DragCorner(this->CurrentPoint, deltaPos.GetData());
        this->Scene->SetDirty(true);
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

void vtkTransferFunctionBoxItem::ClampToValidPosition(double pos[2])
{
  double bounds[4];
  this->GetValidBounds(bounds);
  pos[0] = vtkMath::ClampValue(pos[0], bounds[0], bounds[1]);
  pos[1] = vtkMath::ClampValue(pos[1], bounds[2], bounds[3]);
}

bool vtkTransferFunctionBoxItem::KeyPressEvent(const vtkContextKeyEvent& key)
{
  return Superclass::KeyPressEvent(key);
}

bool vtkTransferFunctionBoxItem::KeyReleaseEvent(const vtkContextKeyEvent& key)
{
  return Superclass::KeyPressEvent(key);
}

void vtkTransferFunctionBoxItem::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);

  auto Box = this->TransferFunctionItem->GetBox();

  os << indent << "Box [x, y, width, height]: [" << Box.GetX() << ", "
     << Box.GetY() << ", " << Box.GetWidth() << ", " << Box.GetHeight()
     << "]\n";
}

void vtkTransferFunctionBoxItem::SetBox(const double x, const double y,
                                        const double width, const double height)
{
  this->IsUpdatingBox = true;
  // Delta position
  double posBottomLeft[2];
  this->BoxPoints->GetPoint(BOTTOM_LEFT, posBottomLeft);

  vtkVector2f deltaPos(x - posBottomLeft[0], y - posBottomLeft[1]);
  this->TransformDataToScreen(deltaPos, deltaPos);

  // Delta dimensions
  double posTopRight[2];
  this->BoxPoints->GetPoint(TOP_RIGHT, posTopRight);

  double deltaSize[2];
  deltaSize[0] = width - (posTopRight[0] - posBottomLeft[0]);
  deltaSize[1] = height - (posTopRight[1] - posBottomLeft[1]);

  this->DragBox(deltaPos.GetX(), deltaPos.GetY());
  this->DragCorner(TOP_RIGHT, deltaSize);
  this->TransferFunctionItem->SetBox(x, y, width, height);
  this->IsUpdatingBox = false;
}

bool vtkTransferFunctionBoxItem::NeedsTextureUpdate()
{
  auto tex = this->Texture.GetPointer();
  return (
    tex->GetMTime() <
      this->TransferFunctionItem->GetColorTransferFunction()->GetMTime() ||
    tex->GetMTime() <
      this->TransferFunctionItem->GetOpacityFunction()->GetMTime());
}

bool vtkTransferFunctionBoxItem::IsInitialized()
{
  return this->TransferFunctionItem &&
         this->TransferFunctionItem->GetColorTransferFunction() &&
         this->TransferFunctionItem->GetOpacityFunction();
}

vtkIdType vtkTransferFunctionBoxItem::FindBoxPoint(double* _pos)
{
  vtkVector2f vpos(_pos[0], _pos[1]);
  this->TransformDataToScreen(vpos, vpos);
  double pos[2] = { vpos.GetX(), vpos.GetY() };

  double tolerance = 1.3;
  double radius2 =
    this->ScreenPointRadius * this->ScreenPointRadius * tolerance * tolerance;

  double screenPos[2];
  this->Transform->TransformPoints(pos, screenPos, 1);
  vtkIdType pointId = -1;
  double minDist = VTK_DOUBLE_MAX;
  const int numberOfPoints = this->GetNumberOfPoints();
  for (vtkIdType i = 0; i < numberOfPoints; ++i) {
    double point[4];
    this->GetControlPoint(i, point);
    vtkVector2f vpos1(point[0], point[1]);
    this->TransformDataToScreen(vpos1, vpos1);
    point[0] = vpos1.GetX();
    point[1] = vpos1.GetY();

    double screenPoint[2];
    this->Transform->TransformPoints(point, screenPoint, 1);
    double distance2 =
      (screenPoint[0] - screenPos[0]) * (screenPoint[0] - screenPos[0]) +
      (screenPoint[1] - screenPos[1]) * (screenPoint[1] - screenPos[1]);

    if (distance2 <= radius2) {
      if (distance2 == 0.) { // we found the best match ever
        return i;
      } else if (distance2 < minDist) { // we found something not too bad, maybe
                                        // we can find closer
        pointId = i;
        minDist = distance2;
      }
    }
  }
  return pointId;
}

void vtkTransferFunctionBoxItem::UpdateInternalBox()
{
  if (!this->IsUpdatingBox && this->TransferFunctionItem) {
    double posBottomLeft[2];
    this->BoxPoints->GetPoint(BOTTOM_LEFT, posBottomLeft);
    double posTopRight[2];
    this->BoxPoints->GetPoint(TOP_RIGHT, posTopRight);

    vtkRectd rect;
    rect.SetX(posBottomLeft[0]);
    rect.SetY(posBottomLeft[1]);
    rect.SetWidth(posTopRight[0] - posBottomLeft[0]);
    rect.SetHeight(posTopRight[1] - posBottomLeft[1]);
    this->TransferFunctionItem->SetBox(rect);
  }
}

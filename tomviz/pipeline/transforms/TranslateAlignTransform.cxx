/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TranslateAlignTransform.h"

#include "AlignWidget.h"
#include "GatedEditorWidget.h"
#include "InputPort.h"
#include "data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include <QJsonArray>

#include <cmath>

namespace {

// We are assuming an image that begins at 0, 0, 0.
vtkIdType imageIndex(const vtkVector3i& incs, const vtkVector3i& pos)
{
  return pos[0] * incs[0] + pos[1] * incs[1] + pos[2] * incs[2];
}

template <typename T>
void applyImageOffsets(T* in, T* out, vtkImageData* image,
                       const QVector<vtkVector2i>& offsets)
{
  int* extents = image->GetExtent();
  vtkVector3i extent(extents[1] - extents[0] + 1, extents[3] - extents[2] + 1,
                     extents[5] - extents[4] + 1);
  vtkVector3i incs(1, 1 * extent[0], 1 * extent[0] * extent[1]);

  // Zero out output
  T* ptr = out;
  for (int i = 0; i < extent[0] * extent[1] * extent[2]; ++i) {
    *ptr++ = 0;
  }

  // Apply per-slice offsets
  for (int i = 0; i < extent[2]; ++i) {
    vtkVector2i offset = offsets[i];
    int idx = imageIndex(incs, vtkVector3i(0, 0, i));
    T* inPtr = in + idx;
    T* outPtr = out + idx;
    for (int y = 0; y < extent[1]; ++y) {
      if (y + offset[1] >= extent[1]) {
        break;
      } else if (y + offset[1] < 0) {
        inPtr += incs[1];
        outPtr += incs[1];
        continue;
      }
      for (int x = 0; x < extent[0]; ++x) {
        if (x + offset[0] >= extent[0]) {
          inPtr += offset[0];
          outPtr += offset[0];
          break;
        } else if (x + offset[0] < 0) {
          ++inPtr;
          ++outPtr;
          continue;
        }
        *(outPtr + offset[0] + incs[1] * offset[1]) = *inPtr;
        ++inPtr;
        ++outPtr;
      }
    }
  }
}

} // namespace

namespace tomviz {
namespace pipeline {

TranslateAlignTransform::TranslateAlignTransform(QObject* parent)
  : TransformNode(parent)
{
  addInput("volume", PortType::ImageData);
  addOutput("output", PortType::ImageData);
  setLabel("Translation Align");
}

bool TranslateAlignTransform::hasPropertiesWidget() const
{
  return true;
}

EditNodeWidget* TranslateAlignTransform::createPropertiesWidget(
  Pipeline* pipeline, QWidget* parent)
{
  return new GatedEditorWidget(
    this, pipeline,
    [this](QWidget* p) -> EditNodeWidget* {
      auto vol = inputPorts()[0]->data().value<VolumeDataPtr>();
      if (!vol || !vol->imageData()) {
        return nullptr;
      }
      vtkSmartPointer<vtkImageData> imageData(vol->imageData());
      return new AlignWidget(this, imageData, p);
    },
    parent);
}

QVector<vtkVector2i> TranslateAlignTransform::offsetsFromAlignmentShifts(
  vtkImageData* image)
{
  QVector<vtkVector2i> offsets;
  if (!image) {
    return offsets;
  }
  auto* fd = image->GetFieldData();
  auto* shifts = fd->GetArray("alignment_shifts");
  auto* size = fd->GetArray("alignment_shifts_image_size");
  int* dims = image->GetDimensions();
  // Binning or cropping changes what a pixel is, so the shifts no longer
  // apply.
  if (!shifts || shifts->GetNumberOfComponents() != 2 ||
      shifts->GetNumberOfTuples() != dims[2] || !size ||
      size->GetNumberOfValues() != 2 || size->GetComponent(0, 0) != dims[0] ||
      size->GetComponent(1, 0) != dims[1]) {
    return offsets;
  }
  for (vtkIdType i = 0; i < shifts->GetNumberOfTuples(); ++i) {
    double x = shifts->GetComponent(i, 0);
    double y = shifts->GetComponent(i, 1);
    if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > dims[0] ||
        std::abs(y) > dims[1]) {
      return QVector<vtkVector2i>();
    }
    offsets.append(vtkVector2i(static_cast<int>(std::lround(x)),
                               static_cast<int>(std::lround(y))));
  }
  return offsets;
}

void TranslateAlignTransform::setAlignOffsets(
  const QVector<vtkVector2i>& newOffsets)
{
  m_offsets.resize(newOffsets.size());
  std::copy(newOffsets.begin(), newOffsets.end(), m_offsets.begin());
}

void TranslateAlignTransform::setDraftAlignOffsets(
  const QVector<vtkVector2i>& newOffsets)
{
  m_draftOffsets.resize(newOffsets.size());
  std::copy(newOffsets.begin(), newOffsets.end(), m_draftOffsets.begin());
}

namespace {

QJsonArray offsetsToJson(const QVector<vtkVector2i>& offsets)
{
  QJsonArray arr;
  for (const auto& v : offsets) {
    arr.append(v.GetX());
    arr.append(v.GetY());
  }
  return arr;
}

QVector<vtkVector2i> jsonToOffsets(const QJsonArray& arr)
{
  QVector<vtkVector2i> out;
  const int pairs = arr.size() / 2;
  out.reserve(pairs);
  for (int i = 0; i < pairs; ++i) {
    out.append(
      vtkVector2i(arr.at(2 * i).toInt(), arr.at(2 * i + 1).toInt()));
  }
  return out;
}

} // namespace

QJsonObject TranslateAlignTransform::serialize() const
{
  auto json = TransformNode::serialize();
  json["offsets"] = offsetsToJson(m_offsets);
  if (!m_draftOffsets.isEmpty()) {
    json["draftOffsets"] = offsetsToJson(m_draftOffsets);
  }
  return json;
}

bool TranslateAlignTransform::deserialize(const QJsonObject& json)
{
  if (!TransformNode::deserialize(json)) {
    return false;
  }
  if (json.contains("offsets")) {
    setAlignOffsets(jsonToOffsets(json.value("offsets").toArray()));
  }
  if (json.contains("draftOffsets")) {
    setDraftAlignOffsets(
      jsonToOffsets(json.value("draftOffsets").toArray()));
  }
  return true;
}

QMap<QString, PortData> TranslateAlignTransform::transform(
  const QMap<QString, PortData>& inputs)
{
  QMap<QString, PortData> result;

  auto inputVolume = inputs["volume"].value<VolumeDataPtr>();
  if (!inputVolume || !inputVolume->isValid()) {
    return result;
  }

  auto* inImage = inputVolume->imageData();

  vtkNew<vtkImageData> outImage;
  outImage->DeepCopy(inImage);

  auto numArrays = inImage->GetPointData()->GetNumberOfArrays();
  for (int i = 0; i < numArrays; ++i) {
    std::string arrayName = inImage->GetPointData()->GetArrayName(i);
    switch (inImage->GetScalarType()) {
      vtkTemplateMacro(applyImageOffsets(
        reinterpret_cast<VTK_TT*>(
          inImage->GetPointData()
            ->GetScalars(arrayName.c_str())
            ->GetVoidPointer(0)),
        reinterpret_cast<VTK_TT*>(
          outImage->GetPointData()
            ->GetScalars(arrayName.c_str())
            ->GetVoidPointer(0)),
        inImage, m_offsets));
    }
  }

  // The aligned images no longer have the shifts stored with them; a
  // further alignment must not start from them again.
  outImage->GetFieldData()->RemoveArray("alignment_shifts");
  outImage->GetFieldData()->RemoveArray("alignment_shifts_image_size");

  auto volume = std::make_shared<VolumeData>(outImage.Get());
  volume->setLabel(inputVolume->label());
  volume->setUnits(inputVolume->units());
  volume->setTiltAngles(inputVolume->tiltAngles());

  result["output"] = PortData(std::any(volume), inputs["volume"].type());
  return result;
}

} // namespace pipeline
} // namespace tomviz

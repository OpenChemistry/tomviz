/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TranslateAlignTransform.h"

#include "AlignWidget.h"
#include "GatedEditorWidget.h"
#include "InputPort.h"
#include "data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include <QJsonArray>

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

  // Apply per-slice offsets; a slice without one is not moved
  for (int i = 0; i < extent[2]; ++i) {
    vtkVector2i offset = i < offsets.size() ? offsets[i] : vtkVector2i(0, 0);
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

  int dims[3];
  inImage->GetDimensions(dims);
  if (!m_offsets.isEmpty() && m_offsets.size() != dims[2]) {
    qWarning("Translation Align has %d offsets for %d slices, so they may "
             "no longer match their slices; reopen Image Alignment "
             "(Manual) to check them. Slices without an offset are left "
             "in place.",
             static_cast<int>(m_offsets.size()), dims[2]);
  }

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

  auto volume = std::make_shared<VolumeData>(outImage.Get());
  volume->setLabel(inputVolume->label());
  volume->setUnits(inputVolume->units());
  volume->setTiltAngles(inputVolume->tiltAngles());

  result["output"] = PortData(std::any(volume), inputs["volume"].type());
  return result;
}

} // namespace pipeline
} // namespace tomviz

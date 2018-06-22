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

#include "TranslateAlignOperator.h"

#include "AlignWidget.h"
#include "DataSource.h"
#include "OperatorResult.h"

#include "vtkIntArray.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkTable.h"

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
  // We know that the input and output images are the same size, with the
  // supplied offsets applied to each slice. Copy the pixels, applying offsets.
  int* extents = image->GetExtent();
  vtkVector3i extent(extents[1] - extents[0] + 1, extents[3] - extents[2] + 1,
                     extents[5] - extents[4] + 1);
  vtkVector3i incs(1, 1 * extent[0], 1 * extent[0] * extent[1]);

  // Zero out our output array, we should do this more intelligently in future.
  T* ptr = out;
  for (int i = 0; i < extent[0] * extent[1] * extent[2]; ++i) {
    *ptr++ = 0;
  }

  // We need to go slice by slice, applying the pixel offsets to the new image.
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
TranslateAlignOperator::TranslateAlignOperator(DataSource* ds, QObject* p)
  : Operator(p), dataSource(ds)
{
  initializeResults();
}

QIcon TranslateAlignOperator::icon() const
{
  return QIcon("");
}

void TranslateAlignOperator::initializeResults() {
  setNumberOfResults(1);
  auto res = resultAt(0);
  res->setName("alignments");
  res->setLabel("Alignments");
  vtkNew<vtkTable> table;
  setResult(0, table);
}

bool TranslateAlignOperator::applyTransform(vtkDataObject* data)
{
  vtkNew<vtkImageData> outImage;
  vtkImageData* inImage = vtkImageData::SafeDownCast(data);
  assert(inImage);
  outImage->DeepCopy(data);
  switch (inImage->GetScalarType()) {
    vtkTemplateMacro(
      applyImageOffsets(reinterpret_cast<VTK_TT*>(inImage->GetScalarPointer()),
                        reinterpret_cast<VTK_TT*>(outImage->GetScalarPointer()),
                        inImage, offsets));
  }
  offsetsToResult();
  data->ShallowCopy(outImage.Get());
  return true;
}

Operator* TranslateAlignOperator::clone() const
{
  TranslateAlignOperator* op = new TranslateAlignOperator(this->dataSource);
  op->setAlignOffsets(this->offsets);
  return op;
}

void TranslateAlignOperator::offsetsToResult()
{
  vtkNew<vtkIntArray> arrX;
  arrX->SetName("X Offset");
  vtkNew<vtkIntArray> arrY;
  arrY->SetName("Y Offset");

  vtkNew<vtkTable> table;
  table->AddColumn(arrX);
  table->AddColumn(arrY);
  table->SetNumberOfRows(offsets.size());

  for (int i = 0; i < offsets.size(); ++i) {
    table->SetValue(i, 0, offsets[i][0]);
    table->SetValue(i, 1, offsets[i][1]);
  }
  setResult(0, table);
}

QJsonObject TranslateAlignOperator::serialize() const
{
  auto json = Operator::serialize();

  QJsonArray offsetArray;
  foreach (auto offset, this->offsets) {
    offsetArray << offset[0] << offset[1];
  }
  json["offsets"] = offsetArray;

  if (m_draftOffsets.size() > 0) {
    QJsonArray draftOffsetArray;
    foreach (auto offset, this->m_draftOffsets) {
      draftOffsetArray << offset[0] << offset[1];
    }
    json["draftOffsets"] = draftOffsetArray;
  }

  return json;
}

bool TranslateAlignOperator::deserialize(const QJsonObject& json)
{
  if (json.contains("offsets") && json["offsets"].isArray()) {
    auto offsetArray = json["offsets"].toArray();
    this->offsets.resize(offsetArray.size() / 2);
    for (int i = 0; i < offsetArray.size() / 2; ++i) {
      this->offsets[i][0] = offsetArray[2 * i].toInt();
      this->offsets[i][1] = offsetArray[2 * i + 1].toInt();
    }
  }

  if (json.contains("draftOffsets") && json["draftOffsets"].isArray()) {
    auto draftOffsetArray = json["draftOffsets"].toArray();
    this->m_draftOffsets.resize(draftOffsetArray.size() / 2);
    for (int i = 0; i < draftOffsetArray.size() / 2; ++i) {
      this->m_draftOffsets[i][0] = draftOffsetArray[2 * i].toInt();
      this->m_draftOffsets[i][1] = draftOffsetArray[2 * i + 1].toInt();
    }
  }

  return true;
}

EditOperatorWidget* TranslateAlignOperator::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> data)
{
  return new AlignWidget(this, data, p);
}

void TranslateAlignOperator::setAlignOffsets(
  const QVector<vtkVector2i>& newOffsets)
{
  this->offsets.resize(newOffsets.size());
  std::copy(newOffsets.begin(), newOffsets.end(), this->offsets.begin());
  emit this->transformModified();
}

void TranslateAlignOperator::setDraftAlignOffsets(
  const QVector<vtkVector2i>& newOffsets)
{
  this->m_draftOffsets.resize(newOffsets.size());
  std::copy(newOffsets.begin(), newOffsets.end(), this->m_draftOffsets.begin());
}
} // namespace tomviz

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

#include "vtkImageData.h"
#include "vtkNew.h"

namespace
{

// We are assuming an image that begins at 0, 0, 0.
vtkIdType imageIndex(const vtkVector3i &incs, const vtkVector3i &pos)
{
  return pos[0] * incs[0] + pos[1] * incs[1] + pos[2] * incs[2];
}

template<typename T>
void applyImageOffsets(T* in, T* out, vtkImageData *image,
                       const QVector<vtkVector2i> &offsets)
{
  // We know that the input and output images are the same size, with the
  // supplied offsets applied to each slice. Copy the pixels, applying offsets.
  int *extents = image->GetExtent();
  vtkVector3i extent(extents[1] - extents[0] + 1,
                     extents[3] - extents[2] + 1,
                     extents[5] - extents[4] + 1);
  vtkVector3i incs(1,
                   1 * extent[0],
                   1 * extent[0] * extent[1]);

  // Zero out our output array, we should do this more intelligently in future.
  T *ptr = out;
  for (int i = 0; i < extent[0] * extent[1] * extent[2]; ++i)
  {
    *ptr++ = 0;
  }

  // We need to go slice by slice, applying the pixel offsets to the new image.
  for (int i = 0; i < extent[2]; ++i)
  {
    vtkVector2i offset = offsets[i];
    int idx = imageIndex(incs, vtkVector3i(0, 0, i));
    T *inPtr = in + idx;
    T* outPtr = out + idx;
    for (int y = 0; y < extent[1]; ++y)
    {
      if (y + offset[1] >= extent[1])
      {
        break;
      }
      else if (y + offset[1] < 0)
      {
        inPtr += incs[1];
        outPtr += incs[1];
        continue;
      }
      for (int x = 0; x < extent[0]; ++x)
      {
        if (x + offset[0] >= extent[0])
        {
          inPtr += offset[0];
          outPtr += offset[0];
          break;
        }
        else if (x + offset[0] < 0)
        {
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
}

namespace tomviz
{
TranslateAlignOperator::TranslateAlignOperator(DataSource *ds, QObject *p)
  : Superclass(p), dataSource(ds)
{
}

QIcon TranslateAlignOperator::icon() const { return QIcon(""); }

bool TranslateAlignOperator::transform(vtkDataObject *data)
{
  vtkNew<vtkImageData> outImage;
  vtkImageData *inImage = vtkImageData::SafeDownCast(data);
  assert(inImage);
  outImage->DeepCopy(data);
  switch (inImage->GetScalarType())
  {
    vtkTemplateMacro(
      applyImageOffsets(reinterpret_cast<VTK_TT*>(inImage->GetScalarPointer()),
                        reinterpret_cast<VTK_TT*>(outImage->GetScalarPointer()),
                        inImage, offsets));
  }
  data->ShallowCopy(outImage.Get());
  return true;
}

Operator* TranslateAlignOperator::clone() const
{
  TranslateAlignOperator *op = new TranslateAlignOperator(this->dataSource);
  op->setAlignOffsets(this->offsets);
  return op;
}

bool TranslateAlignOperator::serialize(pugi::xml_node& ns) const
{
  ns.append_attribute("number_of_offsets").set_value(
      this->offsets.size());
  for (int i = 0; i < this->offsets.size(); ++i)
  {
    pugi::xml_node node = ns.append_child("offset");
    node.append_attribute("slice_number").set_value(i);
    node.append_attribute("x_offset").set_value(this->offsets[i][0]);
    node.append_attribute("y_offset").set_value(this->offsets[i][1]);
  }
  return true;
}

bool TranslateAlignOperator::deserialize(const pugi::xml_node& ns)
{
  int numOffsets = ns.attribute("number_of_offsets").as_int();
  this->offsets.resize(numOffsets);
  for (pugi::xml_node node = ns.child("offset"); node; node = node.next_sibling("offset"))
  {
    int sliceNum = node.attribute("slice_number").as_int();
    int xOffset = node.attribute("x_offset").as_int();
    int yOffset = node.attribute("y_offset").as_int();
    this->offsets[sliceNum][0] = xOffset;
    this->offsets[sliceNum][1] = yOffset;
  }
  return true;
}

EditOperatorWidget* TranslateAlignOperator::getEditorContents(QWidget* p)
{
  return new AlignWidget(this, p);
}

void TranslateAlignOperator::setAlignOffsets(const QVector<vtkVector2i> &newOffsets)
{
  this->offsets.resize(newOffsets.size());
  std::copy(newOffsets.begin(), newOffsets.end(), this->offsets.begin());
}
}

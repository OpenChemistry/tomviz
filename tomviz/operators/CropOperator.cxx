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

#include "CropOperator.h"

#include "EditOperatorWidget.h"
#include "SelectVolumeWidget.h"

#include <vtkExtractVOI.h>
#include <vtkImageData.h>
#include <vtkNew.h>

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>

#include <limits>

namespace {

class CropWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT

public:
  CropWidget(tomviz::CropOperator* source,
             vtkSmartPointer<vtkImageData> imageData, QWidget* p)
    : tomviz::EditOperatorWidget(p), m_operator(source)
  {
    double displayPosition[3] = { 0, 0, 0 };
    double origin[3];
    double spacing[3];
    int extent[6];
    imageData->GetOrigin(origin);
    imageData->GetSpacing(spacing);
    imageData->GetExtent(extent);
    if (source->cropBounds()[0] == std::numeric_limits<int>::min()) {
      source->setCropBounds(extent);
    }
    m_widget = new tomviz::SelectVolumeWidget(
      origin, spacing, extent, source->cropBounds(), displayPosition, this);
    QHBoxLayout* hboxlayout = new QHBoxLayout;
    hboxlayout->addWidget(m_widget);
    setLayout(hboxlayout);
  }

  void applyChangesToOperator() override
  {
    int bounds[6];
    m_widget->getExtentOfSelection(bounds);
    if (m_operator) {
      m_operator->setCropBounds(bounds);
    }
  }

  void dataSourceMoved(double newX, double newY, double newZ) override
  {
    m_widget->dataMoved(newX, newY, newZ);
  }

private:
  QPointer<tomviz::CropOperator> m_operator;
  tomviz::SelectVolumeWidget* m_widget;
};
}

#include "CropOperator.moc"

namespace tomviz {

CropOperator::CropOperator(QObject* p) : Operator(p)
{
  // By default include the entire volume
  for (int i = 0; i < 6; ++i) {
    m_bounds[i] = std::numeric_limits<int>::min();
  }
}

QIcon CropOperator::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqExtractGrid24.png");
}

bool CropOperator::applyTransform(vtkDataObject* data)
{
  vtkNew<vtkExtractVOI> extractor;
  extractor->SetVOI(m_bounds);
  extractor->SetInputDataObject(data);
  extractor->Update();
  extractor->UpdateWholeExtent();
  data->ShallowCopy(extractor->GetOutputDataObject(0));
  return true;
}

Operator* CropOperator::clone() const
{
  CropOperator* other = new CropOperator();
  other->setCropBounds(m_bounds);
  return other;
}

QJsonObject CropOperator::serialize() const
{
  auto json = Operator::serialize();
  QJsonArray bounds;
  for (int i = 0; i < 6; ++i) {
    bounds << m_bounds[i];
  }
  json["bounds"] = bounds;
  return json;
}

bool CropOperator::deserialize(const QJsonObject& json)
{
  if (json.contains("bounds")) {
    auto bounds = json["bounds"].toArray();
    if (bounds.size() == 6) {
      for (int i = 0; i < 6; ++i) {
        m_bounds[i] = bounds[i].toInt();
      }
    }
  }
  return true;
}

void CropOperator::setCropBounds(const int bounds[6])
{
  for (int i = 0; i < 6; ++i) {
    m_bounds[i] = bounds[i];
  }
  emit transformModified();
}

EditOperatorWidget* CropOperator::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> data)
{
  return new CropWidget(this, data, p);
}
}

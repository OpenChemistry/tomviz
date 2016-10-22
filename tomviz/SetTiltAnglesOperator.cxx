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

#include "SetTiltAnglesOperator.h"

#include "DataSource.h"
#include "EditOperatorWidget.h"

#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkTypeInt8Array.h>

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace {
class SetTiltAnglesWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT
public:
  SetTiltAnglesWidget(tomviz::SetTiltAnglesOperator* op,
                      vtkSmartPointer<vtkDataObject> dataObject, QWidget* p)
    : EditOperatorWidget(p), Op(op)
  {
    QMap<size_t, double> tiltAngles = this->Op->tiltAngles();
    QHBoxLayout* baseLayout = new QHBoxLayout;
    this->setLayout(baseLayout);
    this->tabWidget = new QTabWidget;
    baseLayout->addWidget(this->tabWidget);

    QWidget* setAutomaticPanel = new QWidget;

    QGridLayout* layout = new QGridLayout;

    QString description_string =
      "A tomographic \"tilt series\" is a set of projection images taken while "
      "rotating"
      " (\"tilting\") the specimen.  Setting the correct angles is needed for "
      "accurate"
      " reconstruction.\nSet a linearly spaced range of angles by specifying "
      "the start"
      " and end tilt index and start and end angles.  Note, tilt angles can "
      "also be set"
      " in the \"Data Properties\" panel or within Python.";

    vtkImageData* image = vtkImageData::SafeDownCast(dataObject);
    Q_ASSERT(image);

    int extent[6];
    image->GetExtent(extent);
    int totalSlices = extent[5] - extent[4] + 1;
    this->previousTiltAngles.resize(totalSlices);
    double angleIncrement = 1.0;
    if (totalSlices < 60) {
      angleIncrement = 3.0;
    } else if (totalSlices < 80) {
      angleIncrement = 2.0;
    } else if (totalSlices < 120) {
      angleIncrement = 1.5;
    }

    double startAngleValue = -(totalSlices - 1) * angleIncrement / 2.0;
    double endAngleValue = startAngleValue + (totalSlices - 1) * angleIncrement;

    if (tiltAngles.contains(0) && tiltAngles.contains(totalSlices - 1)) {
      startAngleValue = tiltAngles[0];
      endAngleValue = tiltAngles[totalSlices - 1];
    }

    QLabel* descriptionLabel = new QLabel(description_string);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel, 0, 0, 1, 4, Qt::AlignCenter);
    layout->addWidget(new QLabel("Start Image #: "), 1, 0, 1, 1,
                      Qt::AlignCenter);
    this->startTilt = new QSpinBox;
    startTilt->setRange(0, totalSlices - 1);
    startTilt->setValue(0);
    layout->addWidget(startTilt, 1, 1, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("End Image #: "), 2, 0, 1, 1, Qt::AlignCenter);
    this->endTilt = new QSpinBox;
    endTilt->setRange(0, totalSlices - 1);
    endTilt->setValue(totalSlices - 1);
    layout->addWidget(endTilt, 2, 1, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("Set Start Angle: "), 1, 2, 1, 1,
                      Qt::AlignCenter);
    this->startAngle = new QDoubleSpinBox;
    startAngle->setRange(-360.0, 360.0);
    startAngle->setValue(startAngleValue);
    layout->addWidget(startAngle, 1, 3, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("Set End Angle: "), 2, 2, 1, 1,
                      Qt::AlignCenter);
    this->endAngle = new QDoubleSpinBox;
    endAngle->setRange(-360.0, 360.0);
    endAngle->setValue(endAngleValue);
    layout->addWidget(endAngle, 2, 3, 1, 1, Qt::AlignCenter);

    setAutomaticPanel->setLayout(layout);

    QWidget* setFromTablePanel = new QWidget;
    QHBoxLayout* tablePanelLayout = new QHBoxLayout;
    this->tableWidget = new QTableWidget;
    this->tableWidget->setRowCount(totalSlices);
    this->tableWidget->setColumnCount(1);
    tablePanelLayout->addWidget(this->tableWidget);

    vtkFieldData* fd = dataObject->GetFieldData();
    vtkDataArray* tiltArray = nullptr;
    if (fd) {
      tiltArray = fd->GetArray("tilt_angles");
    }
    for (vtkIdType i = 0; i < totalSlices; ++i) {
      QTableWidgetItem* item = new QTableWidgetItem;
      double angle;
      // Make sure to set the previous angles
      if (tiltArray && i < tiltArray->GetNumberOfTuples()) {
        angle = tiltArray->GetTuple1(i);
        this->previousTiltAngles[i] = angle;
      } else {
        angle = 0;
        this->previousTiltAngles[i] = 0;
      }
      // deliberate non-else-if.  We want to override with the previous value
      // from the operator, but we need the previousTiltAngles array set
      // correctly, so the above condition has to be separate.
      if (tiltAngles.contains(i)) {
        angle = tiltAngles[i];
      }
      item->setData(Qt::DisplayRole, QString::number(angle));
      this->tableWidget->setItem(i, 0, item);
    }

    setFromTablePanel->setLayout(tablePanelLayout);

    this->tabWidget->addTab(setAutomaticPanel, "Set by Range");
    this->tabWidget->addTab(setFromTablePanel, "Set Individually");

    baseLayout->setSizeConstraint(QLayout::SetFixedSize);
    p->setFixedSize(670, 330);
  }

  void applyChangesToOperator() override
  {
    if (this->tabWidget->currentIndex() == 0) {
      QMap<size_t, double> tiltAngles = this->Op->tiltAngles();
      int start = startTilt->value();
      int end = endTilt->value();
      if (end == start) {
        tiltAngles[start] = startAngle->value();
      } else {
        double delta =
          (endAngle->value() - startAngle->value()) / (end - start);
        double baseAngle = startAngle->value();
        if (delta < 0) {
          int temp = start;
          start = end;
          end = temp;
          baseAngle = endAngle->value();
        }
        for (int i = 0; start + i <= end; ++i) {
          tiltAngles.insert(start + i, baseAngle + delta * i);
          this->tableWidget->item(i, 0)->setData(
            Qt::DisplayRole, QString::number(tiltAngles[start + i]));
        }
      }
      this->Op->setTiltAngles(tiltAngles);
    } else {
      QMap<size_t, double> tiltAngles;
      for (vtkIdType i = 0; i < this->tableWidget->rowCount(); ++i) {
        QTableWidgetItem* item = this->tableWidget->item(i, 0);
        double val = item->data(Qt::DisplayRole).toDouble();
        if (val != this->previousTiltAngles[i]) {
          tiltAngles[i] = val;
        }
      }
      this->Op->setTiltAngles(tiltAngles);
    }
  }

private:
  QSpinBox* startTilt;
  QSpinBox* endTilt;
  QDoubleSpinBox* startAngle;
  QDoubleSpinBox* endAngle;
  QTableWidget* tableWidget;
  QTabWidget* tabWidget;

  QPointer<tomviz::SetTiltAnglesOperator> Op;
  QVector<double> previousTiltAngles;
};

#include "SetTiltAnglesOperator.moc"
}

namespace tomviz {

SetTiltAnglesOperator::SetTiltAnglesOperator(QObject* p) : Operator(p)
{
}

SetTiltAnglesOperator::~SetTiltAnglesOperator()
{
}

QIcon SetTiltAnglesOperator::icon() const
{
  return QIcon();
}

Operator* SetTiltAnglesOperator::clone() const
{
  SetTiltAnglesOperator* op = new SetTiltAnglesOperator;
  op->setTiltAngles(this->TiltAngles);
  return op;
}

bool SetTiltAnglesOperator::serialize(pugi::xml_node& ns) const
{
  for (auto itr = std::begin(this->TiltAngles);
       itr != std::end(this->TiltAngles); ++itr) {
    pugi::xml_node angleNode = ns.append_child("Angle");
    angleNode.append_attribute("index").set_value((unsigned int)itr.key());
    angleNode.append_attribute("angle").set_value(itr.value());
  }
  return true;
}

bool SetTiltAnglesOperator::deserialize(const pugi::xml_node& ns)
{
  for (auto node = ns.child("Angle"); node; node = node.next_sibling("Angle")) {
    this->TiltAngles.insert(node.attribute("index").as_uint(),
                            node.attribute("angle").as_double());
  }
  return true;
}

EditOperatorWidget* SetTiltAnglesOperator::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> dataObject)
{
  return new SetTiltAnglesWidget(this, dataObject, p);
}

void SetTiltAnglesOperator::setTiltAngles(const QMap<size_t, double>& newAngles)
{
  this->TiltAngles = newAngles;
  emit this->transformModified();
}

bool SetTiltAnglesOperator::applyTransform(vtkDataObject* dataObject)
{
  vtkImageData* image = vtkImageData::SafeDownCast(dataObject);
  if (!image) {
    return false;
  }
  int extent[6];
  image->GetExtent(extent);
  int totalSlices = extent[5] - extent[4] + 1;
  vtkFieldData* fd = dataObject->GetFieldData();
  // Make sure the data is marked as a tilt series
  vtkTypeInt8Array* dataType =
    vtkTypeInt8Array::SafeDownCast(fd->GetArray("tomviz_data_source_type"));
  if (!dataType) {
    vtkNew<vtkTypeInt8Array> array;
    array->SetNumberOfTuples(1);
    array->SetName("tomviz_data_source_type");
    fd->AddArray(array.Get());
    dataType = array.Get();
  }
  // It should already be this value...
  dataType->SetTuple1(0, DataSource::TiltSeries);
  // Set the tilt angles
  vtkDataArray* dataTiltAngles = fd->GetArray("tilt_angles");
  if (!dataTiltAngles) {
    vtkNew<vtkDoubleArray> angles;
    angles->SetNumberOfTuples(totalSlices);
    angles->SetName("tilt_angles");
    fd->AddArray(angles.Get());
    dataTiltAngles = angles.Get();
  } else if (dataTiltAngles->GetNumberOfTuples() < totalSlices) {
    dataTiltAngles->SetNumberOfTuples(totalSlices);
  }
  for (auto itr = std::begin(this->TiltAngles);
       itr != std::end(this->TiltAngles); ++itr) {
    dataTiltAngles->SetTuple(itr.key(), &itr.value());
  }
  return true;
}
}

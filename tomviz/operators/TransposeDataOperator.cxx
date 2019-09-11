/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "TransposeDataOperator.h"

#include "EditOperatorWidget.h"

#include <vtkFloatArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTypeUInt16Array.h>
#include <vtkTypeUInt8Array.h>

#include <QComboBox>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>

namespace {

class TransposeDataWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT

public:
  TransposeDataWidget(tomviz::TransposeDataOperator* source,
                      vtkSmartPointer<vtkImageData> imageData, QWidget* p)
    : tomviz::EditOperatorWidget(p), m_operator(source),
      m_transposeTypesCombo(nullptr)
  {
    Q_UNUSED(imageData)

    // Set up UI...
    auto* transposeLabel = new QLabel("Transpose to:", this);
    transposeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_transposeTypesCombo = new QComboBox(this);
    using TransposeType = tomviz::TransposeDataOperator::TransposeType;
    // This will ensure the combo box indexing matches that of the enum...
    m_transposeTypesCombo->insertItem(static_cast<int>(TransposeType::C),
                                   "C Ordering");
    m_transposeTypesCombo->insertItem(static_cast<int>(TransposeType::Fortran),
                                   "Fortran Ordering");

    auto* vBoxLayout = new QVBoxLayout(this);

    auto* convertHBoxLayout = new QHBoxLayout;
    convertHBoxLayout->addWidget(transposeLabel);
    convertHBoxLayout->addWidget(m_transposeTypesCombo);
    vBoxLayout->addLayout(convertHBoxLayout);

    setLayout(vBoxLayout);
  }

  void applyChangesToOperator() override
  {
    // The combo box and enum indices should match
    using TransposeType = tomviz::TransposeDataOperator::TransposeType;
    m_operator->setTransposeType(
      static_cast<TransposeType>(m_transposeTypesCombo->currentIndex()));
  }

private:
  QPointer<tomviz::TransposeDataOperator> m_operator;
  QComboBox* m_transposeTypesCombo;
};
} // namespace

#include "TransposeDataOperator.moc"

namespace {

template <typename T>
void ReorderArrayC(T* in, T* out, int dim[3])
{
  for (int i = 0; i < dim[0]; ++i) {
    for (int j = 0; j < dim[1]; ++j) {
      for (int k = 0; k < dim[2]; ++k) {
        out[static_cast<size_t>(i * dim[1] + j) * dim[2] + k] =
          in[static_cast<size_t>(k * dim[1] + j) * dim[0] + i];
      }
    }
  }
}

template <typename T>
void ReorderArrayF(T* in, T* out, int dim[3])
{
  for (int i = 0; i < dim[0]; ++i) {
    for (int j = 0; j < dim[1]; ++j) {
      for (int k = 0; k < dim[2]; ++k) {
        out[static_cast<size_t>(k * dim[1] + j) * dim[0] + i] =
          in[static_cast<size_t>(i * dim[1] + j) * dim[2] + k];
      }
    }
  }
}

}

namespace tomviz {

TransposeDataOperator::TransposeDataOperator(QObject* p) : Operator(p)
{
}

QIcon TransposeDataOperator::icon() const
{
  return QIcon();
}

bool TransposeDataOperator::applyTransform(vtkDataObject* data)
{
  auto imageData = vtkImageData::SafeDownCast(data);
  // sanity check
  if (!imageData) {
    qDebug() << "Error in" << __FUNCTION__ << ": imageData is nullptr!";
    return false;
  }

  int dim[3] = { 0, 0, 0 };
  imageData->GetDimensions(dim);

  // We must allocate a new array, and copy the reordered array into it.
  auto scalars = imageData->GetPointData()->GetScalars();
  auto dataPtr = scalars->GetVoidPointer(0);

  vtkNew<vtkImageData> reorderedImageData;
  reorderedImageData->SetDimensions(dim);
  reorderedImageData->AllocateScalars(scalars->GetDataType(),
                                      scalars->GetNumberOfComponents());

  auto outputArray = reorderedImageData->GetPointData()->GetScalars();
  outputArray->SetName(scalars->GetName());

  auto outPtr = outputArray->GetVoidPointer(0);

  switch (m_transposeType) {
    case TransposeType::C:
      switch (scalars->GetDataType()) {
      vtkTemplateMacro(ReorderArrayC(
        reinterpret_cast<VTK_TT*>(dataPtr), reinterpret_cast<VTK_TT*>(outPtr),
        dim));
      default:
        qDebug() << "TransposeType: Unknown data type";
      }
      break;
    case TransposeType::Fortran:
      switch (scalars->GetDataType()) {
      vtkTemplateMacro(ReorderArrayF(
        reinterpret_cast<VTK_TT*>(dataPtr), reinterpret_cast<VTK_TT*>(outPtr),
        dim));
      default:
        qDebug() << "TransposeType: Unknown data type";
      }
      break;
    default:
      qDebug() << "Error in" << __FUNCTION__ << ": unknown transpose type!";
      return false;
  }

  imageData->GetPointData()->RemoveArray(scalars->GetName());
  imageData->GetPointData()->SetScalars(outputArray);

  return true;
}

QJsonObject TransposeDataOperator::serialize() const
{
  auto json = Operator::serialize();
  QJsonValue transposeType(static_cast<int>(m_transposeType));
  json["transposeType"] = transposeType;
  return json;
}

bool TransposeDataOperator::deserialize(const QJsonObject& json)
{
  if (json.contains("transposeType"))
    m_transposeType = static_cast<TransposeType>(json["transposeType"].toInt());

  return true;
}

Operator* TransposeDataOperator::clone() const
{
  auto* other = new TransposeDataOperator();
  other->setTransposeType(m_transposeType);
  return other;
}

EditOperatorWidget* TransposeDataOperator::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> data)
{
  return new TransposeDataWidget(this, data, p);
}

} // namespace tomviz

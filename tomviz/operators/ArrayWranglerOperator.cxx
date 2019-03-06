/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ArrayWranglerOperator.h"

#include "EditOperatorWidget.h"

#include <vtkFloatArray.h>
#include <vtkTypeUInt8Array.h>
#include <vtkTypeUInt16Array.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPointData.h>

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

namespace {

class ArrayWranglerWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT

public:
  ArrayWranglerWidget(tomviz::ArrayWranglerOperator* source,
             vtkSmartPointer<vtkImageData> imageData, QWidget* p)
    : tomviz::EditOperatorWidget(p), m_operator(source),
      m_outputTypes(nullptr), m_componentToKeep(nullptr)
  {
    QLabel* label = new QLabel("Convert to:", this);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_outputTypes = new QComboBox(this);

    using OutputType = tomviz::ArrayWranglerOperator::OutputType;
    // This will ensure the indexing matches that of the enum...
    m_outputTypes->insertItem(static_cast<int>(OutputType::UInt8),  "UInt8");
    m_outputTypes->insertItem(static_cast<int>(OutputType::UInt16), "UInt16");

    QHBoxLayout* hboxlayout = new QHBoxLayout(this);
    hboxlayout->addWidget(label);
    hboxlayout->addWidget(m_outputTypes);

    auto numComponents = imageData->GetPointData()->GetScalars()->GetNumberOfComponents();
    if (numComponents > 1) {
      // Only add this option to the UI if there is more than one component
      QLabel* numComponentsLabel = new QLabel("Component to Keep:");
      numComponentsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
      m_componentToKeep = new QComboBox(this);
      for (int i = 1; i < numComponents + 1; ++i)
        m_componentToKeep->addItem(QString::number(i));
      hboxlayout->addWidget(numComponentsLabel);
      hboxlayout->addWidget(m_componentToKeep);
    }
    else if (m_componentToKeep) {
      // Ensure this is nullptr to communicate a component wasn't chosen
      m_componentToKeep->deleteLater();
      m_componentToKeep = nullptr;
    }

    setLayout(hboxlayout);
  }

  void applyChangesToOperator() override
  {
    // The combo box and enum indices should match
    using OutputType = tomviz::ArrayWranglerOperator::OutputType;
    m_operator->setOutputType(
      static_cast<OutputType>(m_outputTypes->currentIndex()));

    // Set the component we are to keep
    if (m_componentToKeep)
      m_operator->setComponentToKeep(m_componentToKeep->currentIndex());
    else
      m_operator->setComponentToKeep(0);
  }

private:
  QPointer<tomviz::ArrayWranglerOperator> m_operator;
  QComboBox* m_outputTypes;
  QComboBox* m_componentToKeep;
};
} // namespace

#include "ArrayWranglerOperator.moc"

namespace {

template <typename vtkInputDataType, typename vtkOutputArrayType>
void wrangleVtkArrayTypeUnsigned(vtkOutputArrayType* array, int nComps, int nTuples, void* data, double oldrange[2])
{
  // We can't divide by zero...
  //assert(oldrange[1] != oldrange[0]);

  // GetDataTypeValueMax() is supposed to return the native data type
  auto newmax = vtkOutputArrayType::GetDataTypeValueMax();
  using outputType = decltype(newmax);

  // new = ((old - oldmin) / oldrange * newrange) + newmin
  // For unsigned ints, newrange == newmax, and newmin == 0

  // Can be simplified to:
  // new = ((old - oldmin) * multiplier;
  // where multiplier is newmax / oldrange

  double multiplier = newmax / (oldrange[1] - oldrange[0]);

  auto d = static_cast<vtkInputDataType*>(data);
  auto a = static_cast<outputType*>(array->GetVoidPointer(0));
  for (int i = 0; i < nComps * nTuples; ++i) {
    // Add 0.5 before casting to ensure flooring rounds correctly
    a[i] = static_cast<outputType>((d[i] - oldrange[0]) * multiplier + 0.5);
  }
}

template<typename vtkOutputArrayType>
bool applyGenericTransform(vtkDataObject* data)
{
  auto imageData = vtkImageData::SafeDownCast(data);
  // sanity check
  if (!imageData) {
    return false;
  }
  auto scalars = imageData->GetPointData()->GetScalars();

  // Get the range to input into the wrangle function
  double range[2];
  scalars->GetRange(range);

  vtkNew<vtkOutputArrayType> outputArray;
  outputArray->SetNumberOfComponents(scalars->GetNumberOfComponents());
  outputArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
  outputArray->SetName(scalars->GetName());

  switch (scalars->GetDataType()) {
    vtkTemplateMacro(wrangleVtkArrayTypeUnsigned<VTK_TT>(
      outputArray.Get(), scalars->GetNumberOfComponents(),
      scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0), range));
  }

  imageData->GetPointData()->RemoveArray(scalars->GetName());
  imageData->GetPointData()->SetScalars(outputArray);
  return true;
}
} // namespace

namespace tomviz {

ArrayWranglerOperator::ArrayWranglerOperator(QObject* p) : Operator(p) {}

QIcon ArrayWranglerOperator::icon() const
{
  return QIcon();
}

bool ArrayWranglerOperator::applyTransform(vtkDataObject* data)
{
  // Use a template to make it easier to add other types...
  switch (m_outputType) {
    case OutputType::UInt8:
      return applyGenericTransform<vtkTypeUInt8Array>(data);
    case OutputType::UInt16:
      return applyGenericTransform<vtkTypeUInt16Array>(data);
  }
  return false;
}

Operator* ArrayWranglerOperator::clone() const
{
  return new ArrayWranglerOperator();
}

EditOperatorWidget* ArrayWranglerOperator::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> data)
{
  return new ArrayWranglerWidget(this, data, p);
}

} // namespace tomviz

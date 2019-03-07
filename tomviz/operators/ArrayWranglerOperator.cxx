/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ArrayWranglerOperator.h"

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

class ArrayWranglerWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT

public:
  ArrayWranglerWidget(tomviz::ArrayWranglerOperator* source,
                      vtkSmartPointer<vtkImageData> imageData, QWidget* p)
    : tomviz::EditOperatorWidget(p), m_operator(source),
      m_outputTypesCombo(nullptr), m_componentToKeepCombo(nullptr)
  {
    // Set up UI...
    auto* convertLabel = new QLabel("Convert to:", this);
    convertLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_outputTypesCombo = new QComboBox(this);
    using OutputType = tomviz::ArrayWranglerOperator::OutputType;
    // This will ensure the combo box indexing matches that of the enum...
    m_outputTypesCombo->insertItem(static_cast<int>(OutputType::UInt8),
                                   "UInt8");
    m_outputTypesCombo->insertItem(static_cast<int>(OutputType::UInt16),
                                   "UInt16");

    auto* vBoxLayout = new QVBoxLayout(this);

    auto* convertHBoxLayout = new QHBoxLayout;
    convertHBoxLayout->addWidget(convertLabel);
    convertHBoxLayout->addWidget(m_outputTypesCombo);
    vBoxLayout->addLayout(convertHBoxLayout);

    auto numComponents =
      imageData->GetPointData()->GetScalars()->GetNumberOfComponents();
    if (numComponents > 1) {
      // Only add this option to the UI if there is more than one component
      auto* numComponentsLabel = new QLabel("Component to Keep:");
      numComponentsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

      m_componentToKeepCombo = new QComboBox(this);
      // Populate combo box with component selection: 1, 2, 3...
      for (int i = 1; i < numComponents + 1; ++i)
        m_componentToKeepCombo->addItem(QString::number(i));

      auto* componentHBoxLayout = new QHBoxLayout;
      componentHBoxLayout->addWidget(numComponentsLabel);
      componentHBoxLayout->addWidget(m_componentToKeepCombo);
      vBoxLayout->addLayout(componentHBoxLayout);
    } else if (m_componentToKeepCombo) {
      // Ensure this is nullptr to communicate a component wasn't chosen
      m_componentToKeepCombo->deleteLater();
      m_componentToKeepCombo = nullptr;
    }

    setLayout(vBoxLayout);
  }

  void applyChangesToOperator() override
  {
    // The combo box and enum indices should match
    using OutputType = tomviz::ArrayWranglerOperator::OutputType;
    m_operator->setOutputType(
      static_cast<OutputType>(m_outputTypesCombo->currentIndex()));

    // Set the component we are to keep
    if (m_componentToKeepCombo)
      m_operator->setComponentToKeep(m_componentToKeepCombo->currentIndex());
    else
      m_operator->setComponentToKeep(0);
  }

private:
  QPointer<tomviz::ArrayWranglerOperator> m_operator;
  QComboBox* m_outputTypesCombo;
  QComboBox* m_componentToKeepCombo;
};
} // namespace

#include "ArrayWranglerOperator.moc"

namespace {

// This is specifically designed for unsigned integer output types
// The template is disabled for other output types
template <typename vtkInputDataType, typename vtkOutputArrayType,
          typename = std::enable_if<std::is_unsigned<
            decltype(vtkOutputArrayType::GetDataTypeValueMax())>::value>>
void wrangleVtkArrayTypeUnsigned(vtkOutputArrayType* array, int nComps,
                                 int componentToKeep, int nTuples, void* data,
                                 double oldrange[2])
{
  // We can't divide by zero...
  // assert(oldrange[1] != oldrange[0]);

  // GetDataTypeValueMax() is supposed to return the native data type
  auto newmax = vtkOutputArrayType::GetDataTypeValueMax();
  using outputType = decltype(newmax);

  // We will adjust the old range to the new range as follows:
  // new = ((old - oldmin) / oldrange * newrange) + newmin
  // For unsigned ints, newrange == newmax, and newmin == 0

  // Can be simplified to:
  // new = ((old - oldmin) * multiplier;
  // where multiplier is newmax / oldrange

  double multiplier = newmax / (oldrange[1] - oldrange[0]);

  auto d = static_cast<vtkInputDataType*>(data);
  auto a = static_cast<outputType*>(array->GetVoidPointer(0));
  for (vtkIdType i = 0; i < nTuples; ++i) {
    // Add 0.5 before casting to ensure flooring rounds correctly
    // We use a stride here to skip unwanted components in the source
    a[i] = static_cast<outputType>(
      (d[i * nComps + componentToKeep] - oldrange[0]) * multiplier + 0.5);
  }
}

template <typename vtkOutputArrayType>
void applyGenericWrangleTransform(vtkImageData* imageData, int componentToKeep,
                                  double range[2])
{
  auto scalars = imageData->GetPointData()->GetScalars();

  vtkNew<vtkOutputArrayType> outputArray;
  outputArray->SetNumberOfComponents(1); // We will always use one component
  outputArray->SetNumberOfTuples(scalars->GetNumberOfTuples());
  outputArray->SetName(scalars->GetName());

  switch (scalars->GetDataType()) {
    vtkTemplateMacro(wrangleVtkArrayTypeUnsigned<VTK_TT>(
      outputArray.Get(), scalars->GetNumberOfComponents(), componentToKeep,
      scalars->GetNumberOfTuples(), scalars->GetVoidPointer(0), range));
  }

  imageData->GetPointData()->RemoveArray(scalars->GetName());
  imageData->GetPointData()->SetScalars(outputArray);
}
} // namespace

namespace tomviz {

ArrayWranglerOperator::ArrayWranglerOperator(QObject* p) : Operator(p)
{
}

QIcon ArrayWranglerOperator::icon() const
{
  return QIcon();
}

bool ArrayWranglerOperator::applyTransform(vtkDataObject* data)
{
  auto imageData = vtkImageData::SafeDownCast(data);
  // sanity check
  if (!imageData) {
    qDebug() << "Error in" << __FUNCTION__ << ": imageData is nullptr!";
    return false;
  }

  auto scalars = imageData->GetPointData()->GetScalars();
  // One more sanity check
  if (m_componentToKeep >= scalars->GetNumberOfComponents()) {
    qDebug() << "Error in" << __FUNCTION__ << ": componentToKeep,"
             << QString::number(m_componentToKeep) << "is greater than or "
             << "equal to the number of components:"
             << QString::number(scalars->GetNumberOfComponents());
    return false;
  }

  // Get the range to input into the wrangle function
  double range[2];
  scalars->GetFiniteRange(range);

  // Use a template to make it easier to add other types...
  switch (m_outputType) {
    case OutputType::UInt8:
      applyGenericWrangleTransform<vtkTypeUInt8Array>(imageData,
                                                      m_componentToKeep, range);
      break;
    case OutputType::UInt16:
      applyGenericWrangleTransform<vtkTypeUInt16Array>(
        imageData, m_componentToKeep, range);
      break;
    default:
      qDebug() << "Error in" << __FUNCTION__ << ": unknown output type!";
      return false;
  }

  return true;
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

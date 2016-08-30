
#include "pqDoubleSliderPropertyWidget.h"

#include "pqDoubleRangeWidget.h"
#include "pqPropertiesPanel.h"
#include "pqWidgetRangeDomain.h"

#include <QGridLayout>

pqDoubleSliderPropertyWidget::pqDoubleSliderPropertyWidget(
  vtkSMProxy* smProxy, vtkSMProperty* smproperty, QWidget* parentObject)
  : pqPropertyWidget(smProxy, parentObject)
{
  QGridLayout* layout = new QGridLayout(this);
  layout->setMargin(pqPropertiesPanel::suggestedMargin());
  layout->setHorizontalSpacing(pqPropertiesPanel::suggestedHorizontalSpacing());
  layout->setVerticalSpacing(pqPropertiesPanel::suggestedVerticalSpacing());

  pqDoubleRangeWidget* rangeWidget = new pqDoubleRangeWidget(this);
  this->addPropertyLink(rangeWidget, "value", SIGNAL(valueChanged(double)),
                        smproperty, 0);
  layout->addWidget(rangeWidget);

  new pqWidgetRangeDomain(rangeWidget, "minimum", "maximum", smproperty, 0);

  this->setChangeAvailableAsChangeFinished(true);
  std::cout << "Creating slider widget" << std::endl;
}

pqDoubleSliderPropertyWidget::~pqDoubleSliderPropertyWidget()
{
}

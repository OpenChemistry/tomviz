#ifndef pqDoubleSliderPropertyWidget_h
#define pqDoubleSliderPropertyWidget_h

#include "pqPropertyWidget.h"

class pqDoubleSliderPropertyWidget : public pqPropertyWidget
{
  Q_OBJECT

public:
  pqDoubleSliderPropertyWidget(
    vtkSMProxy *smProxy, vtkSMProperty *smproperty, QWidget *parentObject=nullptr);
  virtual ~pqDoubleSliderPropertyWidget();

private:
  Q_DISABLE_COPY(pqDoubleSliderPropertyWidget)
};
#endif

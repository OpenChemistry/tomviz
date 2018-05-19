#include "AdvancedFormatWidget.h"

#include "ui_AdvancedFormatWidget.h"

namespace tomviz {

AdvancedFormatWidget::AdvancedFormatWidget(QWidget *parent)
  : QWidget(parent), m_ui(new Ui::AdvancedFormatWidget)
{
  m_ui->setupUi(this);
}

AdvancedFormatWidget::~AdvancedFormatWidget() = default;

}

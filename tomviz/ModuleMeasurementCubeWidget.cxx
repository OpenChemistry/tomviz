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

#include "ModuleMeasurementCubeWidget.h"
#include "ui_ModuleMeasurementCubeWidget.h"

#include <QDoubleValidator>
#include <QTextStream>

namespace tomviz {

ModuleMeasurementCubeWidget::ModuleMeasurementCubeWidget(QWidget* parent_)
  : QWidget(parent_), m_ui(new Ui::ModuleMeasurementCubeWidget)
{
  m_ui->setupUi(this);
  m_ui->leSideLength->setValidator(new QDoubleValidator(this));

  connect(m_ui->chbAdaptiveScaling, SIGNAL(toggled(bool)), this,
          SIGNAL(adaptiveScalingToggled(const bool)));
  connect(m_ui->leSideLength, &QLineEdit::editingFinished, this,
          [&]{ sideLengthChanged(m_ui->leSideLength->text().toDouble()); });
}

ModuleMeasurementCubeWidget::~ModuleMeasurementCubeWidget() = default;

void ModuleMeasurementCubeWidget::setAdaptiveScaling(const bool choice)
{
  m_ui->chbAdaptiveScaling->setChecked(choice);
}

void ModuleMeasurementCubeWidget::setSideLength(const double length)
{
  m_ui->leSideLength->setText(QString::number(length));
}

void ModuleMeasurementCubeWidget::setLengthUnit(const QString unit)
{
  m_ui->tlLengthUnit->setText(unit);
}

void ModuleMeasurementCubeWidget::setPosition(const double x, const double y,
				       const double z)
{
  QString s;
  QTextStream(&s) << "("
		  << QString::number(x,'f',4) << ", "
		  << QString::number(y,'f',4) << ", "
		  << QString::number(z,'f',4) << ")";
  m_ui->tlPosition->setText(s);
}

void ModuleMeasurementCubeWidget::setPositionUnit(const QString unit)
{
  m_ui->tlPositionUnit->setText(unit);
}

void ModuleMeasurementCubeWidget::onAdaptiveScalingChanged(const bool state)
{
  emit adaptiveScalingToggled(state);
}

void ModuleMeasurementCubeWidget::onSideLengthChanged(const double length)
{
  emit sideLengthChanged(length);
}

}

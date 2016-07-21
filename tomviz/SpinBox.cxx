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

#include "SpinBox.h"

#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSpinBox>

namespace tomviz
{

SpinBox::SpinBox(QWidget *p)
  : QSpinBox(p), pressInUp(false), pressInDown(false)
{
}

void SpinBox::mousePressEvent(QMouseEvent* event)
{
  QSpinBox::mousePressEvent(event);

  QStyleOptionSpinBox opt;
  this->initStyleOption(&opt);

  if (this->style()->subControlRect(QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxUp).contains(event->pos()))
  {
    this->pressInUp = true;
  }
  else if (this->style()->subControlRect(QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxDown).contains(event->pos()))
  {
    this->pressInDown = true;
  }
  else
  {
    this->pressInUp = this->pressInDown = false;
  }
  if (this->pressInUp || this->pressInDown)
  {
    this->connect(this, SIGNAL(valueChanged(int)), this, SIGNAL(editingFinished()));
  }
}

void SpinBox::mouseReleaseEvent(QMouseEvent *event)
{
  QSpinBox::mouseReleaseEvent(event);

  if (this->pressInUp || this->pressInDown)
  {
    this->disconnect(this, SIGNAL(valueChanged(int)), this, SIGNAL(editingFinished()));
  }

  QStyleOptionSpinBox opt;
  this->initStyleOption(&opt);

  if (this->style()->subControlRect(QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxUp).contains(event->pos()))
  {
    if (this->pressInUp)
    {
      emit this->editingFinished();
    }
  }
  else if (this->style()->subControlRect(QStyle::CC_SpinBox, &opt, QStyle::SC_SpinBoxDown).contains(event->pos()))
  {
    if (this->pressInDown)
    {
      emit this->editingFinished();
    }
  }
  this->pressInUp = this->pressInDown = false;
}

}

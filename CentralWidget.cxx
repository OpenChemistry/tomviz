/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "CentralWidget.h"
#include "ui_CentralWidget.h"

#include "pqPipelineSource.h"

#include <QtDebug>

namespace TEM
{

class CentralWidget::CWInternals
{
public:
  Ui::CentralWidget Ui;
};

//-----------------------------------------------------------------------------
CentralWidget::CentralWidget(QWidget* parentObject, Qt::WindowFlags wflags)
  : Superclass(parentObject, wflags),
  Internals(new CentralWidget::CWInternals())
{
  this->Internals->Ui.setupUi(this);
}

//-----------------------------------------------------------------------------
CentralWidget::~CentralWidget()
{
}

//-----------------------------------------------------------------------------
void CentralWidget::setDataSource(pqPipelineSource* source)
{
  qDebug() << "TODO: Generate histogram and show for " <<
    (source? source->getSMName() : QString("(NULL)"));
}

//
//
} // end of namespace TEM

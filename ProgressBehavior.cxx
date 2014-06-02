/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

*****************************************************************************/
#include "ProgressBehavior.h"

#include "pqApplicationCore.h"
#include "pqProgressManager.h"

#include <QProgressDialog>

namespace TEM
{

//-----------------------------------------------------------------------------
ProgressBehavior::ProgressBehavior(QWidget* parentWindow)
  : Superclass(parentWindow)
{
  this->ProgressDialog = new QProgressDialog(
    "In progress...", "Cancel", 0, 100, parentWindow);
  this->ProgressDialog->setAutoClose(true);
  this->ProgressDialog->setAutoReset(false);
  this->ProgressDialog->setMinimumDuration(0); // 0 second.

  pqProgressManager* progressManager =
    pqApplicationCore::instance()->getProgressManager();

  this->connect(progressManager, SIGNAL(enableProgress(bool)),
    SLOT(enableProgress(bool)));
  this->connect(progressManager, SIGNAL(progress(const QString&, int)),
    SLOT(progress(const QString, int)));

}

//-----------------------------------------------------------------------------
ProgressBehavior::~ProgressBehavior()
{
  delete this->ProgressDialog;
}

//-----------------------------------------------------------------------------
void ProgressBehavior::enableProgress(bool enable)
{
  Q_ASSERT(this->ProgressDialog);

  if (enable)
    {
    this->ProgressDialog->setValue(0);
    }
  else
    {
    this->ProgressDialog->reset();
    }
}

//-----------------------------------------------------------------------------
void ProgressBehavior::progress(const QString& message, int progress)
{
  Q_ASSERT(this->ProgressDialog);

  this->ProgressDialog->setLabelText(message);
  this->ProgressDialog->setValue(progress);
}

}

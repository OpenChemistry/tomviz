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

#ifndef tomvizBadStackDialog_h
#define tomvizBadStackDialog_h

#include "ImageStackModel.h"

#include <QDialog>
#include <QScopedPointer>

namespace Ui {

class BadStackDialog;
}

namespace tomviz {

class BadStackDialog : public QDialog
{
  Q_OBJECT

public:
  explicit BadStackDialog(QWidget* parent = nullptr,
                          ImageStackModel* tableModel = nullptr);
  ~BadStackDialog() override;

private slots:

private:
  QScopedPointer<Ui::BadStackDialog> m_ui;
};
}

#endif
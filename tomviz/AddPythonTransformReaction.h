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
#ifndef tomvizAddPythonTransformReaction_h
#define tomvizAddPythonTransformReaction_h

#include "pqReaction.h"
#include <QSpinBox>

namespace tomviz
{
class DataSource;
class OperatorPython;

class AddPythonTransformReaction : public pqReaction
{
  Q_OBJECT
  typedef pqReaction Superclass;

public:
  AddPythonTransformReaction(QAction* parent, const QString &label,
                         const QString &source);
  ~AddPythonTransformReaction();

  OperatorPython* addExpression(DataSource* source = NULL);

  void setInteractive(bool isInteractive) { interactive = isInteractive; }

public slots:
  void updateLastSliceMin(int min); //update minumum value allowed in last slice spinbox
  void updateSliceMax(int a); //update maximum value allowed in of first and last slice
    
protected:
  void updateEnableState();
  void onTriggered() { this->addExpression(); }

private:
  Q_DISABLE_COPY(AddPythonTransformReaction)

  QString scriptLabel;
  QString scriptSource;
  QSpinBox *firstSlice;
  QSpinBox *lastSlice;
  int *shape; //shape of input data
  bool interactive;
};
}

#endif

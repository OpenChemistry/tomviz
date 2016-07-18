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
#ifndef tomvizSetTiltAnglesOperator_h
#define tomvizSetTiltAnglesOperator_h

#include "Operator.h"

#include <QMap>

namespace tomviz
{

class SetTiltAnglesOperator : public Operator
{
  Q_OBJECT

public:
  SetTiltAnglesOperator(QObject *parent = nullptr);
  ~SetTiltAnglesOperator() override;

  QString label() const override { return "Set Tilt Angles"; }
  QIcon icon() const override;
  Operator *clone() const override;
  bool serialize(pugi::xml_node& ns) const override;
  bool deserialize(const pugi::xml_node& ns) override;
  EditOperatorWidget *getEditorContents(QWidget* parent,
      vtkSmartPointer<vtkImageData> data) override;
  bool hasCustomUI() const override { return true; }

  void setTiltAngles(const QMap<size_t, double>& newAngles);
  const QMap<size_t, double>& tiltAngles() const { return this->TiltAngles; }

protected:
  bool applyTransform(vtkDataObject *data) override;

  QMap<size_t, double> TiltAngles;

private:
  Q_DISABLE_COPY(SetTiltAnglesOperator)
};
}

#endif

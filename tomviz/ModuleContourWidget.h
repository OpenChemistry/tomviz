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
#ifndef tomvizModuleContourWidget_h
#define tomvizModuleContourWidget_h

#include <memory>

#include <QWidget>

/**
 * \brief UI layer of ModuleContour.
 *
 * Signals are forwarded to the actual actuators on the mapper in ModuleContour
 * This class is intended to contain only logic related to UI actions.
 */

namespace Ui {
class ModuleContourWidget;
class LightingParametersForm;
}

namespace tomviz {

class ModuleContourWidget : public QWidget
{
  Q_OBJECT

public:
  ModuleContourWidget(QWidget* parent_ = nullptr);
  ~ModuleContourWidget() = default;

  //@{
  /**
   * UI update methods. The actual model state is stored in ModelVolume (either
   * in the mapper or serialized), so the UI needs to be updated if the state
   * changes or when constructing the UI.
   */
  void setLighting(const bool enable);
  void setAmbient(const double value);
  void setDiffuse(const double value);
  void setSpecular(const double value);
  void setSpecularPower(const double value);
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void lightingToggled(const bool state);
  void ambientChanged(const double value);
  void diffuseChanged(const double value);
  void specularChanged(const double value);
  void specularPowerChanged(const double value);
  //@}

private:
  ModuleContourWidget(const ModuleContourWidget&) = delete;
  void operator=(const ModuleContourWidget&) = delete;

  std::shared_ptr<Ui::ModuleContourWidget> m_ui;
  std::shared_ptr<Ui::LightingParametersForm> m_uiLighting;

private slots:
  void onAmbientChanged(const int value);
  void onDiffuseChanged(const int value);
  void onSpecularChanged(const int value);
  void onSpecularPowerChanged(const int value);
};
}
#endif

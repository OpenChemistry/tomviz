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
#ifndef tomvizModuleScaleCubeWidget_h
#define tomvizModuleScaleCubeWidget_h

#include <QScopedPointer>
#include <QWidget>

/**
 * \brief UI layer of ModuleScaleCube.
 *
 * Signals are forwarded to the actual actuators in ModuleScaleCube.
 * This class is intended to contain only logic related to UI actions.
 */

namespace Ui {
class ModuleScaleCubeWidget;
}

namespace tomviz {

class ModuleScaleCubeWidget : public QWidget
{
  Q_OBJECT

public:
  ModuleScaleCubeWidget(QWidget* parent_ = nullptr);
  ~ModuleScaleCubeWidget();

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void adaptiveScalingToggled(const bool state);
  void sideLengthChanged(const double length);
  void annotationToggled(const bool state);
  //@}

public slots:
  //@{
  /**
   * UI update methods. The actual module state is stored in
   * ModuleScaleCube, so the UI needs to be updated if the state changes
   * or when constructing the UI.
   */
  void setAdaptiveScaling(const bool choice);
  void setAnnotation(const bool choice);
  void setLengthUnit(const QString unit);
  void setPositionUnit(const QString unit);
  void setSideLength(const double length);
  void setPosition(const double x, const double y, const double z);
  //@}

private:
  ModuleScaleCubeWidget(const ModuleScaleCubeWidget&) = delete;
  void operator=(const ModuleScaleCubeWidget&) = delete;

  QScopedPointer<Ui::ModuleScaleCubeWidget> m_ui;

private slots:
  void onAdaptiveScalingChanged(const bool state);
  void onSideLengthChanged(const double length);
  void onAnnotationChanged(const bool state);
};
}
#endif

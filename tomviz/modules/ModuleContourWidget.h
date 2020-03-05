/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleContourWidget_h
#define tomvizModuleContourWidget_h

#include <QScopedPointer>
#include <QWidget>

/**
 * \brief UI layer of ModuleContour.
 *
 * Signals are forwarded to  ModuleContour or the proxies. This class is
 * intended
 * to contain only logic related to UI actions.
 */

namespace Ui {
class ModuleContourWidget;
class LightingParametersForm;
} // namespace Ui

namespace tomviz {

class ModuleContourWidget : public QWidget
{
  Q_OBJECT

public:
  ModuleContourWidget(QWidget* parent_ = nullptr);
  ~ModuleContourWidget() override;

  void setIsoRange(double range[2]);
  void setColorByArrayOptions(const QStringList& options);

  //@{
  /**
   * UI update methods. The actual model state is stored in ModuleContour for
   * these parameters, so the UI needs to be updated if the state changes or
   * when constructing the UI.
   */
  void setColorMapData(const bool state);
  void setAmbient(const double value);
  void setDiffuse(const double value);
  void setSpecular(const double value);
  void setSpecularPower(const double value);
  void setIso(const double value);
  void setRepresentation(const QString& representation);
  void setOpacity(const double value);
  void setColor(const QColor& color);
  void setUseSolidColor(const bool state);
  void setColorByArray(const bool state);
  void setColorByArrayName(const QString& name);
  //@}

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void colorMapDataToggled(const bool state);
  void ambientChanged(const double value);
  void diffuseChanged(const double value);
  void specularChanged(const double value);
  void specularPowerChanged(const double value);
  void isoChanged(const double value);
  void representationChanged(const QString& representation);
  void opacityChanged(const double value);
  void colorChanged(const QColor& color);
  void useSolidColorToggled(const bool state);
  void colorByArrayToggled(const bool state);
  void colorByArrayNameChanged(const QString& name);
  //@}

private:
  ModuleContourWidget(const ModuleContourWidget&) = delete;
  void operator=(const ModuleContourWidget&) = delete;

  QScopedPointer<Ui::ModuleContourWidget> m_ui;
  QScopedPointer<Ui::LightingParametersForm> m_uiLighting;
};
} // namespace tomviz
#endif

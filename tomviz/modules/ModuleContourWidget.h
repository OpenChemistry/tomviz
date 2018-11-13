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

class pqPropertyLinks;
class vtkSMProxy;
class vtkSMSourceProxy;

namespace tomviz {

class ModuleContourWidget : public QWidget
{
  Q_OBJECT

public:
  ModuleContourWidget(QWidget* parent_ = nullptr);
  ~ModuleContourWidget() override;

  //@{
  /**
   * UI update methods. The actual model state is stored in ModuleContour for
   * these parameters, so the UI needs to be updated if the state changes or
   * when constructing the UI.
   */
  void setUseSolidColor(const bool useSolid);
  //@}

  /**
   * Link proxy properties to UI.
   */
  void addPropertyLinks(pqPropertyLinks& links, vtkSMProxy* representation,
                        vtkSMSourceProxy* contourFilter);
  void addCategoricalPropertyLinks(pqPropertyLinks& links,
                                   vtkSMProxy* representation);

signals:
  //@{
  /**
   * Forwarded signals.
   */
  void specularPowerChanged(const double value);
  void useSolidColor(const bool value);
  /**
   * All proxy properties should use this signal.
   */
  void propertyChanged();
  //@}

private:
  ModuleContourWidget(const ModuleContourWidget&) = delete;
  void operator=(const ModuleContourWidget&) = delete;

  QScopedPointer<Ui::ModuleContourWidget> m_ui;
  QScopedPointer<Ui::LightingParametersForm> m_uiLighting;
};
} // namespace tomviz
#endif

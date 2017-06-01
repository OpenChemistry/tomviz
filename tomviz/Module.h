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
#ifndef tomvizModule_h
#define tomvizModule_h

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QScopedPointer>

#include <vtkWeakPointer.h>

#include <vtk_pugixml.h>

class QWidget;
class pqAnimationCue;
class vtkImageData;
class vtkSMProxy;
class vtkSMViewProxy;
class vtkPiecewiseFunction;

namespace tomviz {
class DataSource;
/// Abstract parent class for all Modules in tomviz.
class Module : public QObject
{
  Q_OBJECT

public:
  Module(QObject* parent = nullptr);
  ~Module() override;

  /// Returns a  label for this module.
  virtual QString label() const = 0;

  /// Returns an icon to use for this module.
  virtual QIcon icon() const = 0;

  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  virtual bool finalize() = 0;

  /// Returns the visibility for the module.
  virtual bool visibility() const = 0;

  /// Accessors for the data-source and view associated with this Plot.
  DataSource* dataSource() const;
  vtkSMViewProxy* view() const;

  /// Some modules act on one data source and apply coloring from another data
  /// source (e.g. ModuleContour). This method allows a module to designate a
  /// separate data source for coloring, if desired.
  virtual DataSource* colorMapDataSource() const { return dataSource(); }

  /// serialize the state of the module.
  virtual bool serialize(pugi::xml_node& ns) const;
  virtual bool deserialize(const pugi::xml_node& ns);

  /// Modules that use transfer functions should override this method to return
  /// true.
  virtual bool isColorMapNeeded() const { return false; }

  /// Flag indicating whether the module uses a "detached" color map or not.
  /// This is only applicable when isColorMapNeeded() return true.
  void setUseDetachedColorMap(bool);
  bool useDetachedColorMap() const { return m_useDetachedColorMap; }

  /// This will either return the maps from the data source or detached ones
  /// based on the UseDetachedColorMap flag.
  vtkSMProxy* colorMap() const;
  vtkSMProxy* opacityMap() const;
  vtkPiecewiseFunction* gradientOpacityMap() const;
  vtkImageData* transferFunction2D() const;

  /// Returns true if the given proxy is part of the display or data processing
  /// in this module
  virtual bool isProxyPartOfModule(vtkSMProxy* proxy) = 0;

  /// Serialize an animation cue on the given module's proxies.  If the proxy
  /// the
  /// cue is animating is a helper proxy of a proxy in the module the helperKey
  /// parameter should be set to the helper key used to find that helper proxy.
  static bool serializeAnimationCue(pqAnimationCue* cue, const char* proxyName,
                                    pugi::xml_node& ns,
                                    const char* helperKey = nullptr);
  /// This form uses the module and the proxy to look up a proxy name to pass to
  /// the function that actually saves the data to the pugixml structure above.
  static bool serializeAnimationCue(pqAnimationCue* cue, Module* module,
                                    pugi::xml_node& ns,
                                    const char* helperKey = nullptr,
                                    vtkSMProxy* realProxy = nullptr);
  static bool deserializeAnimationCue(Module* module, const pugi::xml_node& ns);
  static bool deserializeAnimationCue(vtkSMProxy* proxy,
                                      const pugi::xml_node& ns);

  virtual bool supportsGradientOpacity() { return false; }

public slots:
  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  virtual bool setVisibility(bool val) = 0;

  bool show() { return setVisibility(true); }
  bool hide() { return setVisibility(false); }

  /// This method is called add the proxies in this module to a
  /// pqProxiesWidget instance. Default implementation simply adds the view
  /// properties. Subclasses should override to add proxies and relevant
  /// properties to the panel.
  virtual void addToPanel(QWidget* panel);

  /// This method is called just prior to removing this Module's properties from
  /// the panel. Subclasses can use it to perform any necessary cleanup such as
  /// disconnecting some signals and slots.
  virtual void prepareToRemoveFromPanel(QWidget* panel);

  /// This method is called when the data source's display position changes.
  virtual void dataSourceMoved(double newX, double newY, double newZ) = 0;

protected:
  /// Modules that use transfer functions for color/opacity should override this
  /// method to set the color map on appropriate representations. This will be
  /// called when the color map proxy is changed, for example, when
  /// setUseDetachedColorMap is toggled.
  virtual void updateColorMap() {}

  /// Returns a string for the save file indicating which proxy within the
  /// module is passed to it.  These should be unique within the module, but
  /// different modules can reuse common strings such as "representation".
  /// getProxyForString is the inverse that should get the proxy given the
  /// string returned from getStringForProxy.  These are used in saving
  ///  animations.
  virtual std::string getStringForProxy(vtkSMProxy* proxy) = 0;
  virtual vtkSMProxy* getProxyForString(const std::string& str) = 0;

signals:
  /// Emitted when the represented DataSource is updated.
  void dataSourceChanged();

  /// Emitted when the UseDetachedColorMap state changes or the detatched color
  /// map is modified
  void colorMapChanged();

  /// Emitted when the module properties are changed in a way that would require
  /// a re-render of the scene to take effect.
  void renderNeeded();

private slots:
  void onColorMapChanged();

private:
  Q_DISABLE_COPY(Module)
  QPointer<DataSource> m_activeDataSource;
  vtkWeakPointer<vtkSMViewProxy> m_view;
  bool m_useDetachedColorMap = false;

  class MInternals;
  const QScopedPointer<MInternals> d;
};
}
#endif

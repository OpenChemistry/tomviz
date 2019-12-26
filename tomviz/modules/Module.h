/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModule_h
#define tomvizModule_h

#include <QObject>

#include <QIcon>
#include <QPointer>
#include <QScopedPointer>

#include <vtkRect.h>
#include <vtkWeakPointer.h>

class QWidget;
class vtkImageData;
class vtkPlane;
class vtkSMProxy;
class vtkSMViewProxy;
class vtkPiecewiseFunction;
class vtkDataObject;

namespace tomviz {

class DataSource;
class MoleculeSource;
class OperatorResult;

/// Abstract parent class for all Modules in tomviz.
class Module : public QObject
{
  Q_OBJECT

public:
  Module(QObject* parent = nullptr);
  ~Module() override;

  /// Transfer function mode (1D or 2D). This enum needs to be synchronized with
  /// the order of the pages in ui->swTransferMode.
  enum TransferMode
  {
    SCALAR = 0,
    GRADIENT_1D,
    GRADIENT_2D
  };
  void setTransferMode(const TransferMode mode);
  TransferMode getTransferMode() const;

  /// Returns a  label for this module.
  virtual QString label() const = 0;

  /// Returns an icon to use for this module.
  virtual QIcon icon() const = 0;

  /// Initialize the module for the data source and view. This is called after a
  /// new module is instantiated. Subclasses override this method to setup the
  /// visualization pipeline for this module.
  virtual bool initialize(DataSource* dataSource, vtkSMViewProxy* view);
  virtual bool initialize(MoleculeSource* moleculeSource, vtkSMViewProxy* view);
  virtual bool initialize(OperatorResult* result, vtkSMViewProxy* view);

  /// Finalize the module. Subclasses should override this method to delete and
  /// release all proxies (and data) created for this module.
  virtual bool finalize() = 0;

  /// Returns the visibility for the module.
  virtual bool visibility() const = 0;

  /// Accessors for the data-source and view associated with this Plot.
  DataSource* dataSource() const;
  MoleculeSource* moleculeSource() const;
  vtkSMViewProxy* view() const;
  // Modules can alternatively use an OperatorResult as DataSource
  OperatorResult* operatorResult() const;

  /// Some modules act on one data source and apply coloring from another data
  /// source (e.g. ModuleContour). This method allows a module to designate a
  /// separate data source for coloring, if desired.
  virtual DataSource* colorMapDataSource() const { return dataSource(); }

  /// serialize the state of the module.
  virtual QJsonObject serialize() const;
  virtual bool deserialize(const QJsonObject& json);

  /// Modules that use transfer functions should override this method to return
  /// true.
  virtual bool isColorMapNeeded() const { return false; }
  virtual bool isOpacityMapped() const { return false; }
  virtual bool areScalarsMapped() const { return false; }

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
  vtkRectd* transferFunction2DBox() const;

  virtual bool supportsGradientOpacity() { return false; }

  /// A description of the data type that will be exported.  For instance if
  /// exporting a mesh, this would return "Mesh".  Returning an empty string
  /// indicates that this module has nothing of interest to be exported.
  virtual QString exportDataTypeString() { return ""; }

  /// Returns the data to export for this visualization module.
  virtual vtkDataObject* dataToExport();

  /// Returns the active scalars of the module
  int activeScalars() const { return m_activeScalars; }
  static const int DEFAULT_SCALARS;

signals:

  /// Emitted when the transfer function mode changed in the concrete
  /// module (which would require external UI components to be adjusted).
  void transferModeChanged(const int mode);

public slots:
  /// Set the visibility for this module. Subclasses should override this method
  /// show/hide all representations created for this module.
  virtual bool setVisibility(bool val);

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

  // This method is called when the active scalars for the module change
  virtual void setActiveScalars(int scalars);

protected:
  /// Modules that use transfer functions for color/opacity should override this
  /// method to set the color map on appropriate representations. This will be
  /// called when the color map proxy is changed, for example, when
  /// setUseDetachedColorMap is toggled.
  virtual void updateColorMap() {}

signals:
  /// Emitted when the represented DataSource is updated.
  void dataSourceChanged();

  /// Emitted when the UseDetachedColorMap state changes or the detatched color
  /// map is modified
  void colorMapChanged();

  /// Emitted when the module properties are changed in a way that would require
  /// a re-render of the scene to take effect.
  void renderNeeded();

  /// Emitted when a clipping plane has been created
  void clipFilterUpdated(vtkPlane* plane, bool newFilter);
  void updateClipFilter(vtkPlane* plane, bool newFilter);

  /// Emitted when the module explicitly requires the opacity of the color map
  /// to be enforced. This will cause the colormap to be detached, and the
  /// "Separate Color Map" box to be checked and disabled. In practice, this is
  ///  only useful to make Orthogonal and regular slices transparent.
  void opacityEnforced(bool);

  void visibilityChanged(bool);

private slots:
  void onColorMapChanged();

private:
  Q_DISABLE_COPY(Module)
  QPointer<DataSource> m_activeDataSource;
  QPointer<MoleculeSource> m_activeMoleculeSource;
  QPointer<OperatorResult> m_operatorResult;
  vtkWeakPointer<vtkSMViewProxy> m_view;
  bool m_useDetachedColorMap = false;

  class MInternals;
  const QScopedPointer<MInternals> d;
  int m_activeScalars = DEFAULT_SCALARS;
};
} // namespace tomviz
#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizVolumeManager_h
#define tomvizVolumeManager_h

#include <QObject>

#include <QMap>
#include <QSet>

class vtkSMViewProxy;

namespace tomviz {

class Module;
class ModuleVolume;

/// Singleton to keep track of the volume modules added to each view,
/// So that a vtkMultiVolume can be used to fix volume overlapping issues.
class VolumeManager : public QObject
{
  Q_OBJECT

  typedef QObject Superclass;

public:
  static VolumeManager& instance();
  void allowMultiVolume(bool allow, vtkSMViewProxy* view);

signals:
  void volumeCountChanged(vtkSMViewProxy* view, int count);
  void usingMultiVolumeChanged(vtkSMViewProxy* view, bool enabled);
  void allowMultiVolumeChanged(vtkSMViewProxy* view, bool allow);

private slots:
  void onModuleAdded(Module* module);
  void onModuleRemoved(Module* module);
  void onVisibilityChanged(bool visible);

private:
  Q_DISABLE_COPY(VolumeManager)
  VolumeManager(QObject* parent = nullptr);
  ~VolumeManager();

  void multiVolumeOn(vtkSMViewProxy* view);
  void multiVolumeOff(vtkSMViewProxy* view);

  class Internals;
  QScopedPointer<Internals> d;
};

} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizModuleAnimations_h
#define tomvizModuleAnimations_h

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>

namespace tomviz {

class ModuleAnimation;

namespace pipeline {
class Node;
class Pipeline;
} // namespace pipeline

/// The visualization animations running in the current session.
///
/// These used to live in the Animation Helper dialog, but they outlive
/// it (it is created once and only hidden) and they are saved with the
/// state file, so the list belongs somewhere both the dialog and the
/// state serializer can reach. Same arrangement as CameraViewpoints.
///
/// Animations delete themselves when their node goes away, so the list
/// holds guarded pointers and drops the dead ones as it goes.
class ModuleAnimations : public QObject
{
  Q_OBJECT

public:
  static ModuleAnimations& instance();

  /// Take on `animation`, which must already be constructed against the
  /// node it animates.
  void add(ModuleAnimation* animation);

  /// Drop one animation. No-op if it is not in the list.
  void remove(ModuleAnimation* animation);

  /// The animations still alive, dead entries pruned.
  QList<ModuleAnimation*> animations();

  bool isEmpty();
  void clear();

  /// The pipeline is passed in because nodes are saved by the id it
  /// assigns them, which is the same id the rest of the state file uses.
  /// Size each volume's render quality for the most expensive opacity
  /// curve its animation passes through, so an exported sequence holds one
  /// sharpness throughout. Undone by releaseExportQuality().
  void pinExportQuality();
  void releaseExportQuality();

  QJsonObject serialize(pipeline::Pipeline* pipeline) const;

  /// Rebuild the animations described by `json` against the nodes of
  /// `pipeline`. Entries naming a node that is not there any more are
  /// skipped. Replaces whatever is currently in the list.
  void deserialize(const QJsonObject& json, pipeline::Pipeline* pipeline);

signals:
  void changed();

private:
  Q_DISABLE_COPY(ModuleAnimations)

  ModuleAnimations() = default;

  void prune();

  QList<QPointer<ModuleAnimation>> m_animations;
};

} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAnimationSerializer_h
#define tomvizAnimationSerializer_h

#include <QJsonObject>

namespace tomviz {

namespace pipeline {
class Pipeline;
}

/// Serialization helper for the top-level "animation" section of a
/// Tomviz state file: the frame count, the saved camera viewpoints and
/// the visualization animations. Both state formats go through here, in
/// the same spirit as ViewsLayoutsSerializer.
class AnimationSerializer
{
public:
  /// Write the animation section into @a doc. Safe only where
  /// pqApplicationCore has been initialized.
  static void save(QJsonObject& doc);

  /// Restore the animation section from @a doc. Visualization
  /// animations name their nodes by the id the pipeline assigns, so
  /// @a pipeline must already be loaded. A file with no animation
  /// section leaves an empty animation state rather than whatever the
  /// previous file put there.
  static void restore(const QJsonObject& doc, pipeline::Pipeline* pipeline);
};

} // namespace tomviz

#endif

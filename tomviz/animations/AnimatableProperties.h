/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAnimatableProperties_h
#define tomvizAnimatableProperties_h

#include <QList>
#include <QString>

#include <functional>

namespace tomviz {

class ModuleAnimation;

namespace pipeline {
class Node;
}

/// The bounds and defaults of a property's range controls.
struct PropertyRange
{
  /// The caption in front of the spin boxes, e.g. "Iso value:".
  QString label = "Range:";
  int decimals = 2;
  double lo = 0.0;
  double hi = 1.0;
  double start = 0.0;
  double stop = 1.0;
};

/// One numeric property of a visualization that the Animation Helper
/// can sweep from one value to another: how it is listed, what it
/// applies to, the range its controls offer, how to build the
/// animation, and how to recognise one so an existing row can be put
/// back into the controls. Adding a property is one entry in
/// animatableProperties(); the dialog never names properties itself.
///
/// The opacity curve morph is not in the table: it is keyframed by
/// viewpoint rather than swept between two numbers, and has its own
/// page in the dialog.
struct AnimatableProperty
{
  /// Stable identifier, also used by RecordedChange::controlProperty and
  /// as the combo's item data.
  QString id;
  /// The combo text, which can depend on the node (a slice reads by
  /// index while axis aligned and by position while custom).
  std::function<QString(pipeline::Node*)> label;
  std::function<bool(pipeline::Node*)> applies;
  std::function<PropertyRange(pipeline::Node*)> range;
  std::function<ModuleAnimation*(pipeline::Node*, double start, double stop)>
    make;
  /// If @a animation was built by this property, fill in its values.
  std::function<bool(ModuleAnimation* animation, double& start, double& stop)>
    matches;
};

/// Every property the dialog can author, in the order they are listed.
const QList<AnimatableProperty>& animatableProperties();

/// The property with the given id, or null.
const AnimatableProperty* animatableProperty(const QString& id);

/// The property that built @a animation and its values, or null for an
/// animation no property owns (the curve morph, recorded animations).
const AnimatableProperty* animatablePropertyOf(ModuleAnimation* animation,
                                               double& start, double& stop);

/// True if any property applies to @a node.
bool hasAnimatableProperties(pipeline::Node* node);

} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "PipelineUtils.h"

#include "InputPort.h"
#include "Link.h"
#include "Node.h"
#include "OutputPort.h"
#include "Pipeline.h"
#include "PortType.h"
#include "SinkGroupNode.h"
#include "SinkNode.h"
#include "SourceNode.h"
#include "TransformNode.h"

namespace tomviz {
namespace pipeline {

OutputPort* findBranchTip(Node* node)
{
  if (!node) {
    return nullptr;
  }

  // If it's a sink or sink group, step upstream to the node feeding it
  Node* start = node;
  while (dynamic_cast<SinkNode*>(start) ||
         dynamic_cast<SinkGroupNode*>(start)) {
    auto upstream = start->upstreamNodes();
    if (upstream.isEmpty()) {
      return nullptr;
    }
    start = upstream.first();
  }

  if (start->outputPorts().isEmpty()) {
    return nullptr;
  }

  OutputPort* tip = start->outputPorts()[0];

  // Walk downstream through transforms to the end of this branch.
  // SinkGroupNode is treated as terminal (not followed).
  Node* current = start;
  while (true) {
    TransformNode* nextTransform = nullptr;
    for (auto* downstream : current->downstreamNodes()) {
      if (dynamic_cast<SinkGroupNode*>(downstream)) {
        continue; // skip sink groups
      }
      if (auto* xf = dynamic_cast<TransformNode*>(downstream)) {
        nextTransform = xf;
        break;
      }
    }
    if (!nextTransform || nextTransform->outputPorts().isEmpty()) {
      break;
    }
    tip = nextTransform->outputPorts()[0];
    current = nextTransform;
  }

  return tip;
}

OutputPort* findTipOutputPort(Pipeline* pipeline, Node* contextNode)
{
  if (!pipeline) {
    return nullptr;
  }

  // If we have context, find the tip of the branch containing that node
  if (contextNode && pipeline->nodes().contains(contextNode)) {
    auto* tip = findBranchTip(contextNode);
    if (tip) {
      return tip;
    }
  }

  // Fallback: first source's branch
  for (auto* node : pipeline->nodes()) {
    if (auto* src = dynamic_cast<SourceNode*>(node)) {
      return findBranchTip(src);
    }
  }

  return nullptr;
}

OutputPort* sinkAttachPort(Pipeline* pipeline, OutputPort* targetPort,
                           InputPort* input)
{
  if (!pipeline || !targetPort || !input ||
      !isPortTypeCompatible(targetPort->type(), input->acceptedTypes())) {
    return nullptr;
  }

  // The target port is a group's own passthrough (the group is what's
  // selected): connect straight to it.
  if (qobject_cast<SinkGroupNode*>(targetPort->node())) {
    return targetPort;
  }

  // A compatible group already hangs off the target port: reuse its
  // matching passthrough.
  for (auto* link : targetPort->links()) {
    auto* group = qobject_cast<SinkGroupNode*>(link->to()->node());
    if (!group) {
      continue;
    }
    int idx = group->inputPorts().indexOf(link->to());
    if (idx >= 0 && idx < group->outputPorts().size() &&
        isPortTypeCompatible(group->outputPorts()[idx]->type(),
                             input->acceptedTypes())) {
      return group->outputPorts()[idx];
    }
  }

  auto* group = new SinkGroupNode();
  PortType groupType = isVolumeType(targetPort->type())
                         ? PortType::ImageData
                         : targetPort->type();
  group->addPassthrough(targetPort->name(), groupType);
  pipeline->addNode(group);
  pipeline->createLink(targetPort, group->inputPorts()[0]);
  return group->outputPorts()[0];
}

} // namespace pipeline
} // namespace tomviz

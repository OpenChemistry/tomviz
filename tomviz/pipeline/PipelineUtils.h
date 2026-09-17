/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePipelineUtils_h
#define tomvizPipelinePipelineUtils_h

namespace tomviz {
namespace pipeline {

class InputPort;
class Node;
class OutputPort;
class Pipeline;

/// Find the tip output port of the branch containing the given node.
/// Walks downstream from the node through TransformNodes to find the end
/// of that specific branch. If the node is a SinkNode, walks upstream first
/// to find the feeding source/transform, then walks downstream from there.
OutputPort* findBranchTip(Node* node);

/// Find the tip output port using contextNode to select the right branch.
/// If contextNode is null, falls back to the first source in the pipeline.
OutputPort* findTipOutputPort(
  Pipeline* pipeline, Node* contextNode);

/// The output port a new sink's @a input should be linked to so the sink
/// hangs off a SinkGroupNode on @a targetPort: the port itself when it
/// already belongs to a group, the matching passthrough of a compatible
/// group already linked to it, or otherwise a new group created on it.
/// Returns nullptr when @a input does not accept @a targetPort's type.
OutputPort* sinkAttachPort(Pipeline* pipeline, OutputPort* targetPort,
                           InputPort* input);

} // namespace pipeline
} // namespace tomviz

#endif

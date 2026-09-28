# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""User-facing base classes for schema-v2 Python nodes.

These are the tomviz_pipeline kernel classes under their historical
tomviz.nodes names (SourceNode / TransformNode). A schema-v2 operator
script defines exactly one subclass; the C++ side (PythonSource /
PythonTransform) instantiates it, injects the progress/cancel wrapper,
and calls produce() / transform(). Datasets are the numpy-backed
tomviz_pipeline classes in every runtime; create_dataset() returns a
fresh one."""

from tomviz_pipeline._legacy_nodes import (  # noqa: F401
    Node,
    SourceNode,
    TransformNode,
)

__all__ = ['Node', 'SourceNode', 'TransformNode']

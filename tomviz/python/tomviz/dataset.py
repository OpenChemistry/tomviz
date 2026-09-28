# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""The dataset classes passed to tomviz operator scripts.

These are the numpy-backed tomviz_pipeline classes — the same objects
in-app, in the CLI, and under external execution. The app converts
to/from vtkImageData only at the C++ script boundary
(tomviz._boundary)."""

from tomviz_pipeline.dataset import Dataset, LegacyDataset  # noqa: F401

__all__ = ['Dataset', 'LegacyDataset']

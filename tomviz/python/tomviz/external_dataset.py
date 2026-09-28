# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Backwards-compatible home of the dataset classes.

User scripts written before the tomviz-pipeline split imported Dataset
and LegacyDataset from here; they now live in tomviz_pipeline.dataset
(re-exported as tomviz.dataset)."""

from tomviz_pipeline.dataset import Dataset, LegacyDataset  # noqa: F401

__all__ = ['Dataset', 'LegacyDataset']

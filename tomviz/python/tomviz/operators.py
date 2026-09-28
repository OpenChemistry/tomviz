# -*- coding: utf-8 -*-

###############################################################################
# This source file is part of the Tomviz project, https://tomviz.org/.
# It is released under the 3-Clause BSD License, see "LICENSE".
###############################################################################
"""Operator base classes for tomviz scripts.

Since the in-app pipeline switched to the numpy-backed
tomviz_pipeline datasets, the implementation is the library's —
in-app and headless execution share one behavior. This module remains
so `import tomviz.operators` resolves here inside the application (and
in environments that install the app package)."""

from tomviz_pipeline.operators import (  # noqa: F401
    CancelableOperator,
    CompletableOperator,
    Operator,
    Progress,
)

__all__ = ['Operator', 'CancelableOperator', 'CompletableOperator',
           'Progress']

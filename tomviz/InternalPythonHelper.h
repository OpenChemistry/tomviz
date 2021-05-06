/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizInternalPythonHelper_h
#define tomvizInternalPythonHelper_h

#include <QScopedPointer>

#include "PythonUtilities.h"

namespace tomviz {

class InternalPythonHelper
{
public:
  InternalPythonHelper();
  ~InternalPythonHelper();

  Python::Module loadModule(const QString& script,
                            const QString& name = "tomviz_auto_generated");

  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz

#endif // tomvizInternalPythonHelper_h

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "Logger.h"
#include "PythonUtilities.h"

#include <QDebug>

namespace tomviz {

void Logger::critical(const QString& msg)
{
  qCritical() << msg;
}
} // namespace tomviz

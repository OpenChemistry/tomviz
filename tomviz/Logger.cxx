#include "Logger.h"
#include "PythonUtilities.h"

#include <QDebug>

namespace tomviz {

void Logger::critical(const QString& msg)
{
  qCritical() << msg;
}
}

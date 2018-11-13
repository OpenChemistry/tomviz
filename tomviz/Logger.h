/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLogger_h
#define tomvizLogger_h

#include <QString>

namespace tomviz {

class Logger
{
public:
  static void critical(const QString& msg);
};
} // namespace tomviz

#endif

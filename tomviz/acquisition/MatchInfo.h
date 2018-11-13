/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizMatchInfo_h
#define tomvizMatchInfo_h

#include <QList>

namespace tomviz {

struct CapGroup
{
  CapGroup(const QString& name_, const QString& text)
    : name(name_), capturedText(text)
  {
  }
  QString name;
  QString capturedText;
};

struct MatchInfo
{
  bool matched;
  QList<CapGroup> groups;
};
}

#endif

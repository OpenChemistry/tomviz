#ifndef tomvizMatchInfo_h
#define tomvizMatchInfo_h

#include <QList>

namespace tomviz {

struct CapGroup
{
  CapGroup(QString name_, QString text) : name(name_), capturedText(text) {}
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

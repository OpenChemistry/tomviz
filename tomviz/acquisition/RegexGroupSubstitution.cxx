/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "RegexGroupSubstitution.h"

#include <QDataStream>

namespace tomviz {

RegexGroupSubstitution::RegexGroupSubstitution() = default;

RegexGroupSubstitution::RegexGroupSubstitution(const QString& groupName,
                                               const QString& regex,
                                               const QString& substitution)
  : m_groupName(groupName), m_regex(regex), m_substitution(substitution)
{}

RegexGroupSubstitution::~RegexGroupSubstitution() = default;

void RegexGroupSubstitution::registerType()
{

  static bool registered = false;
  if (!registered) {
    registered = true;
    qRegisterMetaTypeStreamOperators<tomviz::RegexGroupSubstitution>(
      "tomviz::RegexGroupSubstitution");
  }
}

QDataStream& operator<<(QDataStream& out, const RegexGroupSubstitution& conn)
{
  out << conn.groupName() << conn.regex() << conn.substitution();

  return out;
}

QDataStream& operator>>(QDataStream& in, RegexGroupSubstitution& conn)
{
  QString groupName, regex, substitution;
  in >> groupName >> regex >> substitution;
  conn.setGroupName(groupName);
  conn.setRegex(regex);
  conn.setSubstitution(substitution);

  return in;
}
} // namespace tomviz

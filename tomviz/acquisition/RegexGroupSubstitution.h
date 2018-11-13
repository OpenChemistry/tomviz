/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizRegexGroupSubstitution_h
#define tomvizRegexGroupSubstitution_h

#include <QMetaType>
#include <QString>

namespace tomviz {

class RegexGroupSubstitution
{
public:
  RegexGroupSubstitution();
  RegexGroupSubstitution(const QString& groupName, const QString& regex,
                         const QString& substitution);
  ~RegexGroupSubstitution();

  QString groupName() const { return m_groupName; }
  void setGroupName(const QString& groupName) { m_groupName = groupName; }
  QString regex() const { return m_regex; }
  void setRegex(const QString& regex) { m_regex = regex; }
  QString substitution() const { return m_substitution; }
  void setSubstitution(const QString& substitution)
  {
    m_substitution = substitution;
  }

  static void registerType();

private:
  QString m_groupName;
  QString m_regex;
  QString m_substitution;
};
} // namespace tomviz

Q_DECLARE_METATYPE(tomviz::RegexGroupSubstitution)

#endif

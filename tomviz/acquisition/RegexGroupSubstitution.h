/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/

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

  QString groupName() const { return m_groupName; };
  void setGroupName(const QString& groupName) { m_groupName = groupName; };
  QString regex() const { return m_regex; };
  void setRegex(const QString& regex) { m_regex = regex; };
  QString substitution() const { return m_substitution; };
  void setSubstitution(const QString& substitution)
  {
    m_substitution = substitution;
  };

  static void registerType();

private:
  QString m_groupName;
  QString m_regex;
  QString m_substitution;
};
}

Q_DECLARE_METATYPE(tomviz::RegexGroupSubstitution)

#endif

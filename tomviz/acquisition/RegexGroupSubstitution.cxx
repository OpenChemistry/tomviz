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

#include "RegexGroupSubstitution.h"

#include <QDataStream>

namespace tomviz {

RegexGroupSubstitution::RegexGroupSubstitution() = default;

RegexGroupSubstitution::RegexGroupSubstitution(const QString& groupName,
                                               const QString& regex,
                                               const QString& substitution)
  : m_groupName(groupName), m_regex(regex), m_substitution(substitution)
{
}

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
}

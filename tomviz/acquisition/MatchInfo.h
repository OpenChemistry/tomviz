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

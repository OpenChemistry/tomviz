/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __RecentFilesMenu_h
#define __RecentFilesMenu_h

#include "pqRecentFilesMenu.h"

namespace TEM
{
/// Extends pqRecentFilesMenu to add support to open a data file customized for
/// MatViz.
class RecentFilesMenu : public pqRecentFilesMenu
{
  Q_OBJECT;
  typedef pqRecentFilesMenu Superclass;
public:
  RecentFilesMenu(QMenu& menu, QObject* parent=NULL);
  virtual ~RecentFilesMenu();

protected:
  virtual pqPipelineSource* createReader(
    const QString& readerGroup,
    const QString& readerName,
    const QStringList& files,
    pqServer* server) const;
private:
  Q_DISABLE_COPY(RecentFilesMenu);
};
}
#endif

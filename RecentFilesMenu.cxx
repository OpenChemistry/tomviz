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
#include "RecentFilesMenu.h"

#include "DataSource.h"
#include "LoadDataReaction.h"
#include "pqPipelineSource.h"
#include "pqSettings.h"
#include "Utilities.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineController.h"

#include <QMenu>

namespace TEM
{
//-------------------------------------------------------------------------
RecentFilesMenu::RecentFilesMenu(QMenu& menu, QObject* parentObject)
  : Superclass(parentObject)
{
  this->connect(&menu, SIGNAL(aboutToShow()), SLOT(aboutToShowMenu()));
  this->connect(&menu, SIGNAL(triggered(QAction*)), SLOT(triggeredMenu(QAction*)));
}

//-------------------------------------------------------------------------
RecentFilesMenu::~RecentFilesMenu()
{
}

//-------------------------------------------------------------------------
void RecentFilesMenu::aboutToShowMenu()
{
  QMenu* menu = qobject_cast<QMenu*>(this->sender());
  Q_ASSERT(menu);
  menu->clear();

  QSettings* settings = pqApplicationCore::instance()->settings();
  QString recent = settings->value("recentFiles").toString();
  pugi::xml_document doc;
  if (recent.isEmpty() || !doc.load(recent.toUtf8().data()))
    {
    QAction* actn = menu->addAction("Empty");
    actn->setEnabled(false);
    return;
    }

  for (pugi::xml_node node = doc.child("DataSource"); node; node = node.next_sibling("DataSource"))
    {

    }

}

//-------------------------------------------------------------------------
void RecentFilesMenu::triggeredMenu(QAction* actn)
{
}

//
////-------------------------------------------------------------------------
//pqPipelineSource* RecentFilesMenu::createReader(
//    const QString& readerGroup,
//    const QString& readerName,
//    const QStringList& files,
//    pqServer* server) const
//{
//  vtkNew<vtkSMParaViewPipelineController> controller;
//  pqPipelineSource* reader = this->Superclass::createReader(
//    readerGroup, readerName, files, server);
//  if (reader)
//    {
//    DataSource* dataSource = LoadDataReaction::createDataSource(reader);
//    controller->UnRegisterProxy(reader->getProxy());
//    reader = NULL;
//    return TEM::convert<pqPipelineSource*>(dataSource->producer());
//    }
//  return NULL;
//}

}

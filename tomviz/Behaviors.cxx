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
#include "Behaviors.h"

#include "AddRenderViewContextMenuBehavior.h"
#include "MoveActiveObject.h"
#include "pqAlwaysConnectedBehavior.h"
#include "pqApplicationCore.h"
#include "pqDefaultViewBehavior.h"
#include "pqInterfaceTracker.h"
#include "pqPersistentMainWindowStateBehavior.h"
#include "pqPluginManager.h"
#include "pqQtMessageHandlerBehavior.h"
#include "pqStandardPropertyWidgetInterface.h"
#include "pqStandardViewFrameActionsImplementation.h"
#include "pqViewStreamingBehavior.h"
#include "ProgressBehavior.h"
//#include "ScaleActorBehavior.h"
#include "vtkSMReaderFactory.h"
#include "vtkSMTransferFunctionPresets.h"
#include "vtkNew.h"
#include "vtkSMSettings.h"

#include <QMainWindow>
#include <QApplication>
#include <QFile>

// Import the generated header to load our custom plugin
#include "pvextensions/tomvizExtensions_Plugin.h"

PV_PLUGIN_IMPORT_INIT(tomvizExtensions)

const char* const settings =
"{"
"   \"settings\" : {"
"      \"RenderViewSettings\" : {"
"         \"LODThreshold\" : 102400.0,"
//"         \"ShowAnnotation\" : 1,"
"         \"UseDisplayLists\" : 1"
"      }"
"   },"
"   \"lookup_tables\" : {"
"      \"PVLookupTable\" : {"
"         \"ColorSpace\" : 0,"
"         \"NanColor\" : [ 1, 0, 0 ],"
"         \"RGBPoints\" : [ 37.353103637695312, 0, 0, 0, 276.82882690429688, 1, 1, 1 ]"
"      }"
"   }"
"}"
;

namespace tomviz
{
//-----------------------------------------------------------------------------
Behaviors::Behaviors(QMainWindow* mainWindow)
  : Superclass(mainWindow)
{
  Q_ASSERT(mainWindow);
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "JPEGSeriesReader");
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "PNGSeriesReader");
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "TIFFSeriesReader");
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "TVRawImageReader");
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "MRCSeriesReader");
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "XdmfReader");
  vtkSMReaderFactory::AddReaderToWhitelist("sources", "CSVReader");

  PV_PLUGIN_IMPORT(tomvizExtensions)

  vtkSMSettings::GetInstance()->AddCollectionFromString(settings, 0.0);

  // Register ParaView interfaces.
  pqInterfaceTracker* pgm = pqApplicationCore::instance()->interfaceTracker();

  // * add support for ParaView properties panel widgets.
  pgm->addInterface(new pqStandardPropertyWidgetInterface(pgm));

  // * register standard types of view-frame actions.
  pgm->addInterface(new pqStandardViewFrameActionsImplementation(pgm));

  // Load plugins distributed with application.
  pqApplicationCore::instance()->loadDistributedPlugins();

  new pqQtMessageHandlerBehavior(this);
//  new pqDefaultViewBehavior(this);
  new pqAlwaysConnectedBehavior(this);
  new pqViewStreamingBehavior(this);
  new pqPersistentMainWindowStateBehavior(mainWindow);
  new tomviz::ProgressBehavior(mainWindow);
  //new tomviz::ScaleActorBehavior(this);

  new tomviz::AddRenderViewContextMenuBehavior(this);

  new tomviz::MoveActiveObject(this);

  vtkNew<vtkSMTransferFunctionPresets> presets;
  bool needToAddMatplotlibColormaps = true;
  for (unsigned i = 0; i < presets->GetNumberOfPresets(); ++i)
  {
    if (presets->GetPresetName(i) == QString("Viridis (matplotlib)"))
    {
      needToAddMatplotlibColormaps = false;
      break;
    }
  }
  if (needToAddMatplotlibColormaps)
  {
    QString colorMapFile = getMatplotlibColorMapFile();
    presets->ImportPresets(colorMapFile.toStdString().c_str());
  }

  // this will trigger the logic to setup reader/writer factories, etc.
  pqApplicationCore::instance()->loadConfigurationXML("<xml/>");
}

//-----------------------------------------------------------------------------
Behaviors::~Behaviors()
{
}

QString Behaviors::getMatplotlibColorMapFile()
{
  QString path = QApplication::applicationDirPath() + "/../share/tomviz/matplotlib_cmaps.json";
  QFile file(path);
  if (file.exists())
  {
    return path;
  }
// On OSX the above doesn't work in a build tree.  It is fine
// for superbuilds, but the following is needed in the build tree
// since the executable is three levels down in bin/tomviz.app/Contents/MacOS/
#ifdef __APPLE__
  else
  {
    path = QApplication::applicationDirPath() + "/../../../../share/tomviz/scripts/matplotlib_cmaps.json";
    return path;
  }
#else
  return "";
#endif
}

} // end of namespace tomviz

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
#include "ProgressBehavior.h"
#include "ViewFrameActions.h"

#include <pqAlwaysConnectedBehavior.h>
#include <pqApplicationCore.h>
#include <pqDefaultViewBehavior.h>
#include <pqInterfaceTracker.h>
#include <pqPersistentMainWindowStateBehavior.h>
#include <pqPluginManager.h>
#include <pqQtMessageHandlerBehavior.h>
#include <pqStandardPropertyWidgetInterface.h>
#include <pqStandardViewFrameActionsImplementation.h>
#include <pqViewStreamingBehavior.h>
#include <vtkNew.h>
#include <vtkSMReaderFactory.h>
#include <vtkSMSettings.h>
#include <vtkSMTransferFunctionPresets.h>
#include <vtkSmartPointer.h>

#include <QApplication>
#include <QFile>
#include <QMainWindow>

#include <sstream>

// Import the generated header to load our custom plugin
#include "pvextensions/tomvizExtensions_Plugin.h"

PV_PLUGIN_IMPORT_INIT(tomvizExtensions)

const char* const settings = "{"
                             "   \"settings\" : {"
                             "      \"RenderViewSettings\" : {"
                             "         \"LODThreshold\" : 102400.0,"
                             //"         \"ShowAnnotation\" : 1,"
                             "         \"UseDisplayLists\" : 1"
                             "      }"
                             "   }"
                             "}";

namespace tomviz {

Behaviors::Behaviors(QMainWindow* mainWindow) : Superclass(mainWindow)
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

  pgm->addInterface(new ViewFrameActions(pgm));
  // Load plugins distributed with application.
  pqApplicationCore::instance()->loadDistributedPlugins();

  new pqQtMessageHandlerBehavior(this);
  new pqDefaultViewBehavior(this);
  new pqAlwaysConnectedBehavior(this);
  new pqViewStreamingBehavior(this);
  new pqPersistentMainWindowStateBehavior(mainWindow);
  new tomviz::ProgressBehavior(mainWindow);
  // new tomviz::ScaleActorBehavior(this);

  new tomviz::AddRenderViewContextMenuBehavior(this);

  this->MoveActiveBehavior = new tomviz::MoveActiveObject(this);

  // Set the default color map from a preset.
  this->setDefaultColorMapFromPreset("Plasma_17");

  // this will trigger the logic to setup reader/writer factories, etc.
  pqApplicationCore::instance()->loadConfigurationXML("<xml/>");
}

Behaviors::~Behaviors()
{
}

QString Behaviors::getMatplotlibColorMapFile()
{
  QString path = QApplication::applicationDirPath() +
                 "/../share/tomviz/matplotlib_cmaps.json";
  QFile file(path);
  if (file.exists()) {
    return path;
  }
// On OSX the above doesn't work in a build tree.  It is fine
// for superbuilds, but the following is needed in the build tree
// since the executable is three levels down in bin/tomviz.app/Contents/MacOS/
#ifdef __APPLE__
  else {
    path = QApplication::applicationDirPath() +
           "/../../../../share/tomviz/matplotlib_cmaps.json";
    return path;
  }
#else
  return "";
#endif
}

vtkSMTransferFunctionPresets* Behaviors::getPresets()
{
  vtkSMTransferFunctionPresets* presets = vtkSMTransferFunctionPresets::New();
  bool needToAddMatplotlibColormaps = true;
  for (unsigned i = 0; i < presets->GetNumberOfPresets(); ++i) {
    if (presets->GetPresetName(i) == QString("Viridis_17")) {
      needToAddMatplotlibColormaps = false;
      break;
    }
  }
  if (needToAddMatplotlibColormaps) {
    QString colorMapFile = getMatplotlibColorMapFile();
    presets->ImportPresets(colorMapFile.toStdString().c_str());
  }

  return presets;
}

void Behaviors::setDefaultColorMapFromPreset(const char* name)
{
  // We need to search for the preset index.
  vtkSmartPointer<vtkSMTransferFunctionPresets> presets;
  presets.TakeReference(this->getPresets());

  unsigned int presetIndex = presets->GetNumberOfPresets();
  for (unsigned int i = 0; i < presets->GetNumberOfPresets(); ++i) {
    vtkStdString presetName = presets->GetPresetName(i);
    if (presetName == name) {
      presetIndex = i;
    }
  }

  // If the index is valid, a preset was found, and we use it.
  if (presetIndex < presets->GetNumberOfPresets()) {
    // Construct settings JSON for the default color map
    vtkStdString defaultColorMapJSON = presets->GetPresetAsString(presetIndex);
    std::ostringstream ss;
    ss << "{\n"
       << "  \"lookup_tables\" : {\n"
       << "    \"PVLookupTable\" : \n"
       << defaultColorMapJSON << "}\n}\n";

    // Add a settings collection for this
    vtkSMSettings::GetInstance()->AddCollectionFromString(ss.str().c_str(),
                                                          1.0);
  }
}

} // end of namespace tomviz

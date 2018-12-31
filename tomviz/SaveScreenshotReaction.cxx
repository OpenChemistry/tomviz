/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SaveScreenshotReaction.h"

#include "MainWindow.h"
#include "SaveScreenshotDialog.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqCoreUtilities.h>
#include <pqSaveScreenshotReaction.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkNew.h>
#include <vtkPVProxyDefinitionIterator.h>
#include <vtkSMParaViewPipelineController.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyDefinitionManager.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSaveScreenshotProxy.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewLayoutProxy.h>
#include <vtkSMViewProxy.h>

#include <QDebug>
#include <QFileDialog>
#include <QRegularExpression>

namespace tomviz {

SaveScreenshotReaction::SaveScreenshotReaction(QAction* a, MainWindow* mw)
  : pqReaction(a), m_mainWindow(mw)
{}

void SaveScreenshotReaction::saveScreenshot(MainWindow* mw)
{
  auto view = pqActiveObjects::instance().activeView();
  if (!view) {
    qDebug() << "Cannnot save image. No active view.";
    return;
  }
  QSize viewSize = view->getSize();

  SaveScreenshotDialog ssDialog(mw);
  ssDialog.setSize(viewSize.width(), viewSize.height());

  auto pxm =
    vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
  if (pxm->GetProxyDefinitionManager()) {
    auto iter =
      pxm->GetProxyDefinitionManager()->NewSingleGroupIterator("palettes");
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal();
         iter->GoToNextItem()) {
      auto prototype = pxm->GetPrototypeProxy("palettes", iter->GetProxyName());
      if (prototype) {
        ssDialog.addPalette(prototype->GetXMLLabel(), prototype->GetXMLName());
      }
    }
    iter->Delete();
  }

  if (ssDialog.exec() != QDialog::Accepted) {
    return;
  }

  QString lastUsedExt;
  // Load the most recently used file extensions from QSettings, if available.
  auto settings = pqApplicationCore::instance()->settings();
  if (settings->contains("extensions/ScreenshotExtension")) {
    lastUsedExt = settings->value("extensions/ScreenshotExtension").toString();
  }

  QStringList filters;
  filters << "PNG image (*.png)";
  filters << "BMP image (*.bmp)";
  filters << "TIFF image (*.tif)";
  filters << "PPM image (*.ppm)";
  filters << "JPG image (*.jpg)";

  QFileDialog file_dialog(nullptr, "Save Screenshot:");
  file_dialog.setFileMode(QFileDialog::AnyFile);
  file_dialog.setNameFilters(filters);
  file_dialog.setObjectName("FileSaveScreenshotDialog");
  file_dialog.setAcceptMode(QFileDialog::AcceptSave);

  if (file_dialog.exec() != QDialog::Accepted) {
    return;
  }
  QStringList filenames = file_dialog.selectedFiles();
  QString format = file_dialog.selectedNameFilter();
  QString filename = filenames[0];
  int startPos = format.indexOf("(") + 1;
  int n = format.indexOf(")") - startPos;
  QString extensionString = format.mid(startPos, n);
  QStringList extensions =
    extensionString.split(QRegularExpression(" ?\\*"), QString::SkipEmptyParts);
  bool hasExtension = false;
  for (QString& str : extensions) {
    if (filename.endsWith(str)) {
      hasExtension = true;
    }
  }
  if (!hasExtension) {
    filename = QString("%1%2").arg(filename, extensions[0]);
  }

  QFileInfo fileInfo = QFileInfo(filename);
  lastUsedExt = QString("*.") + fileInfo.suffix();
  settings->setValue("extensions/ScreenshotExtension", lastUsedExt);

  // This is working around an issue on macOS, currently saved screenshots are
  // twice the requested size on a retinal display. The device pixel ratio will
  // be 1 apart from on a retinal display where it will be 2. This will need to
  // be removed when this bug is resolved more correctly.
  int dpr = 1;
  if (mw) {
    dpr = mw->devicePixelRatio();
  }
  QSize size = QSize(ssDialog.width() / dpr, ssDialog.height() / dpr);
  QString palette = ssDialog.palette();

  bool makeTransparentBackground = false;
  if (palette == "Transparent Background") {
    makeTransparentBackground = true;
    palette = "";
  }

  auto colorPalette = pxm->GetProxy("global_properties", "ColorPalette");
  vtkSmartPointer<vtkSMProxy> clone;
  if (colorPalette && !palette.isEmpty()) {
    // save current property values
    clone.TakeReference(
      pxm->NewProxy(colorPalette->GetXMLGroup(), colorPalette->GetXMLName()));
    clone->Copy(colorPalette);

    auto chosenPalette = pxm->NewProxy("palettes", palette.toLatin1().data());
    colorPalette->Copy(chosenPalette);
    chosenPalette->Delete();
  }

  vtkSMViewProxy* viewProxy = view->getViewProxy();
  vtkSMViewLayoutProxy* layout = vtkSMViewLayoutProxy::FindLayout(viewProxy);

  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("misc", "SaveScreenshot"));
  vtkSMSaveScreenshotProxy* shProxy =
    vtkSMSaveScreenshotProxy::SafeDownCast(proxy);
  if (!shProxy) {
    // TODO something went horribly wrong
    return;
  }

  vtkNew<vtkSMParaViewPipelineController> controller;
  controller->PreInitializeProxy(shProxy);
  vtkSMPropertyHelper(shProxy, "View").Set(viewProxy);
  vtkSMPropertyHelper(shProxy, "Layout").Set(layout);
  controller->PostInitializeProxy(shProxy);

  int resolution[2];
  resolution[0] = size.width();
  resolution[1] = size.height();
  vtkSMPropertyHelper(shProxy, "ImageResolution").Set(resolution, 2);
  vtkSMPropertyHelper(shProxy, "OverrideColorPalette")
    .Set(palette.toLocal8Bit().data());
  vtkSMPropertyHelper(shProxy, "TransparentBackground")
    .Set(makeTransparentBackground);

  shProxy->WriteImage(filename.toLocal8Bit().data());
}
} // namespace tomviz

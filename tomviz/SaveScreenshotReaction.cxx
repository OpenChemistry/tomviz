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

#include "SaveScreenshotReaction.h"

#include "MainWindow.h"

#include <pqActiveObjects.h>
#include <pqApplicationCore.h>
#include <pqCoreUtilities.h>
#include <pqFileDialog.h>
#include <pqSaveScreenshotReaction.h>
#include <pqSettings.h>
#include <pqView.h>
#include <vtkPVProxyDefinitionIterator.h>
#include <vtkSMProxy.h>
#include <vtkSMProxyDefinitionManager.h>
#include <vtkSMProxyManager.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMViewProxy.h>

#include <QComboBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>


namespace tomviz
{

SaveScreenshotReaction::SaveScreenshotReaction(QAction *a, MainWindow *mw)
  : Superclass(a), mainWindow(mw)
{
}

SaveScreenshotReaction::~SaveScreenshotReaction()
{
}

void SaveScreenshotReaction::saveScreenshot(MainWindow *mw)
{
  pqView* view = pqActiveObjects::instance().activeView();
  if (!view)
  {
    qDebug() << "Cannnot save image. No active view.";
    return;
  }
  QSize viewSize = view->getSize();

  QDialog ssDialog(mw);
  ssDialog.setWindowTitle("Save Screenshot Options");
  QVBoxLayout *vLayout = new QVBoxLayout;

  QLabel *label = new QLabel("Select resolution for the image to save");
  vLayout->addWidget(label);

  QHBoxLayout *dimensionsLayout = new QHBoxLayout;
  QSpinBox *width = new QSpinBox;
  width->setRange(50, std::numeric_limits<int>::max());
  width->setValue(viewSize.width());
  label = new QLabel("x");
  QSpinBox *height = new QSpinBox;
  height->setRange(50, std::numeric_limits<int>::max());
  height->setValue(viewSize.height());
  QPushButton *lockAspectButton = new QPushButton(QIcon(":/pqWidgets/Icons/pqOctreeData16.png"),"");
  dimensionsLayout->addWidget(width);
  dimensionsLayout->addWidget(label);
  dimensionsLayout->addWidget(height);
  dimensionsLayout->addWidget(lockAspectButton);
  vLayout->addItem(dimensionsLayout);

  label = new QLabel("Override Color Palette");
  vLayout->addWidget(label);

  QComboBox *paletteBox = new QComboBox;
  paletteBox->addItem("Current Palette", "");
  vLayout->addWidget(paletteBox);

  QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
  QObject::connect(buttonBox, &QDialogButtonBox::accepted, &ssDialog, &QDialog::accept);
  QObject::connect(buttonBox, &QDialogButtonBox::rejected, &ssDialog, &QDialog::reject);
  vLayout->addWidget(buttonBox);

  ssDialog.setLayout(vLayout);

  vtkSMSessionProxyManager* pxm =
      vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
  if (pxm->GetProxyDefinitionManager())
  {
    vtkPVProxyDefinitionIterator* iter =
      pxm->GetProxyDefinitionManager()->NewSingleGroupIterator("palettes");
    for (iter->InitTraversal(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      vtkSMProxy* prototype = pxm->GetPrototypeProxy("palettes",
        iter->GetProxyName());
      if (prototype)
      {
        paletteBox->addItem(prototype->GetXMLLabel(),
          prototype->GetXMLName());
      }
    }
    iter->Delete();
  }
  paletteBox->addItem("Transparent Background", "Transparent Background");

  if (ssDialog.exec() != QDialog::Accepted)
  {
    return;
  }

  QString lastUsedExt;
  // Load the most recently used file extensions from QSettings, if available.
  pqSettings* settings = pqApplicationCore::instance()->settings();
  if (settings->contains("extensions/ScreenshotExtension"))
  {
    lastUsedExt =
      settings->value("extensions/ScreenshotExtension").toString();
  }

  QString filters;
  filters += "PNG image (*.png)";
  filters += ";;BMP image (*.bmp)";
  filters += ";;TIFF image (*.tif)";
  filters += ";;PPM image (*.ppm)";
  filters += ";;JPG image (*.jpg)";
  pqFileDialog file_dialog(NULL,
    pqCoreUtilities::mainWidget(),
    tr("Save Screenshot:"), QString(), filters);
  file_dialog.setRecentlyUsedExtension(lastUsedExt);
  file_dialog.setObjectName("FileSaveScreenshotDialog");
  file_dialog.setFileMode(pqFileDialog::AnyFile);
  if (file_dialog.exec() != QDialog::Accepted)
  {
    return;
  }

  QString file = file_dialog.getSelectedFiles()[0];
  QFileInfo fileInfo = QFileInfo( file );
  lastUsedExt = QString("*.") + fileInfo.suffix();
  settings->setValue("extensions/ScreenshotExtension", lastUsedExt);

  QSize size = QSize(width->value(), height->value());
  QString palette = paletteBox->itemData(paletteBox->currentIndex()).toString();

  bool makeTransparentBackground = false;
  bool wasTransparent = vtkSMViewProxy::GetTransparentBackground();
  if (palette == "Transparent Background")
  {
    std::cout << "Trying to make transparent" << std::endl;
    makeTransparentBackground = true;
    palette = "";
    vtkSMViewProxy::SetTransparentBackground(1);
  }

  vtkSMProxy* colorPalette = pxm->GetProxy(
    "global_properties", "ColorPalette");
  vtkSmartPointer<vtkSMProxy> clone;
  if (colorPalette && !palette.isEmpty())
  {
    // save current property values
    clone.TakeReference(pxm->NewProxy(colorPalette->GetXMLGroup(),
        colorPalette->GetXMLName()));
    clone->Copy(colorPalette);

    vtkSMProxy* chosenPalette =
      pxm->NewProxy("palettes", palette.toLatin1().data());
    colorPalette->Copy(chosenPalette);
    chosenPalette->Delete();
  }

  pqSaveScreenshotReaction::saveScreenshot(file,
    size, 100, false);

  // restore color palette.
  if (clone)
  {
    colorPalette->Copy(clone);
    pqApplicationCore::instance()->render();
  }
  if (makeTransparentBackground)
  {
    vtkSMViewProxy::SetTransparentBackground(wasTransparent);
  }

}

}

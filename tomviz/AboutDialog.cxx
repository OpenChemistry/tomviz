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

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "ActiveObjects.h"
#include "MainWindow.h"
#include "ModuleManager.h"
#include "PythonUtilities.h"
#include "tomvizConfig.h"

#include <pqApplicationCore.h>
#include <pqSettings.h>

#include <vtkNew.h>
#include <vtkPVConfig.h>
#include <vtkPVOpenGLInformation.h>
#include <vtkVersion.h>

#include <QString>
#include <QTreeWidget>

#include <itkConfigure.h>

namespace tomviz {

static inline void AddItem(QTreeWidget* tree, const QString& name,
                           const QString& value)
{
  QTreeWidgetItem* item = new QTreeWidgetItem(tree);
  item->setText(0, name);
  item->setText(1, value);
}

static inline void AddPythonAttributeItem(QTreeWidget* tree,
                                          const QString& itemName,
                                          const char* module,
                                          const char* attribute)
{
  Python::initialize();

  bool itemAdded = false;
  Python py;
  Python::Module mod = py.import(QString(module));
  if (mod.isValid()) {
    Python::Function func = mod.findFunction(QString(attribute));
    if (func.isValid()) {
      QString value = func.toString();
      AddItem(tree, itemName, value);
      itemAdded = true;
    }
  }

  if (!itemAdded) {
    AddItem(tree, itemName, "<not found>");
  }
}

AboutDialog::AboutDialog(MainWindow* mw)
  : QDialog(mw), m_ui(new Ui::AboutDialog)
{
  m_ui->setupUi(this);

  QString tomvizVersion(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0) {
    tomvizVersion.append("-").append(TOMVIZ_VERSION_EXTRA);
  }

  QTreeWidget* tree = m_ui->information;
  AddItem(tree, "Tomviz Version", tomvizVersion);
  AddItem(tree, "ParaView Version", QString(PARAVIEW_VERSION_FULL));
  AddItem(tree, "VTK Version", QString(VTK_VERSION));
  AddItem(tree, "ITK Version", QString(ITK_VERSION_STRING));

  vtkNew<vtkPVOpenGLInformation> OpenGLInfo;
  OpenGLInfo->CopyFromObject(NULL);

  AddItem(tree, "OpenGL Vendor",
          QString::fromStdString(OpenGLInfo->GetVendor()));
  AddItem(tree, "OpenGL Version",
          QString::fromStdString(OpenGLInfo->GetVersion()));
  AddItem(tree, "OpenGL Renderer",
          QString::fromStdString(OpenGLInfo->GetRenderer()));

  AddItem(tree, "Qt Version", QString(QT_VERSION_STR));

  AddPythonAttributeItem(tree, "Python Version", "sys", "version");
  AddPythonAttributeItem(tree, "NumPy Version", "numpy", "__version__");
  AddPythonAttributeItem(tree, "SciPy Version", "scipy", "__version__");

  AddPythonAttributeItem(tree, "Python Path", "sys", "prefix");
  AddPythonAttributeItem(tree, "NumPy Path", "numpy", "__file__");
  AddPythonAttributeItem(tree, "SciPy Path", "scipy", "__file__");

  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

AboutDialog::~AboutDialog() = default;
}

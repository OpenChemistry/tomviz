/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "PythonUtilities.h"
#include "tomvizConfig.h"

#include <vtkPVConfig.h>

#include <vtkNew.h>
#include <vtkRenderWindow.h>

#include <vtk_glew.h>

#include <QString>
#include <QTreeWidget>

namespace tomviz {

namespace {

// Build up the JSON object to store details for the about dialog.
void buildJson(QJsonObject& details)
{
  QString tomvizVersion(TOMVIZ_VERSION);
  if (QString(TOMVIZ_VERSION_EXTRA).size() > 0) {
    tomvizVersion.append("-").append(TOMVIZ_VERSION_EXTRA);
  }
  details["tomvizVersion"] = tomvizVersion;
  details["paraviewVersion"] = QString(PARAVIEW_VERSION_FULL);
  details["qtVersion"] = QString(QT_VERSION_STR);
}

// Get OpenGL information, focus on VTK's view.
void buildOpenGL(QJsonObject& details)
{
  vtkNew<vtkRenderWindow> window;
  window->SetOffScreenRendering(1);
  window->Render();
  details["openglVendor"] =
    QString(reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
  details["openglVersion"] =
    QString(reinterpret_cast<const char*>(glGetString(GL_VERSION)));
  details["openglRenderer"] =
    QString(reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
  details["openglShaderVersion"] = QString(
    reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));
}

void buildPython(QJsonObject& details)
{
  Python::initialize();
  Python py;
  Python::Module modSys = py.import("sys");
  if (modSys.isValid()) {
    Python::Function prefix = modSys.findFunction("prefix");
    if (prefix.isValid()) {
      details["pythonPrefix"] = prefix.toString();
    }
  }
  Python::Module modPlatform = py.import("platform");
  if (modPlatform.isValid()) {
    Python::Function ver = modPlatform.findFunction("python_version");
    Python::Tuple tuple(0);
    if (ver.isValid()) {
      details["pythonVersion"] = ver.call(tuple).toString();
    }
  }
  Python::Module modNumpy = py.import("numpy");
  if (modNumpy.isValid()) {
    Python::Function ver = modNumpy.findFunction("__version__");
    Python::Function path = modNumpy.findFunction("__file__");
    if (ver.isValid() && path.isValid()) {
      details["numpyVersion"] = ver.toString();
      details["numpyPath"] = path.toString();
    }
  }
  Python::Module modScipy = py.import("scipy");
  if (modScipy.isValid()) {
    Python::Function ver = modScipy.findFunction("__version__");
    Python::Function path = modScipy.findFunction("__file__");
    if (ver.isValid() && path.isValid()) {
      details["scipyVersion"] = ver.toString();
      details["scipyPath"] = path.toString();
    }
  }
}

void AddItem(QTreeWidget* tree, const QString& name, const QString& value)
{
  QTreeWidgetItem* item = new QTreeWidgetItem(tree);
  item->setText(0, name);
  item->setText(1, value);
}
} // namespace

AboutDialog::AboutDialog(QWidget* parent)
  : QDialog(parent), m_ui(new Ui::AboutDialog)
{
  m_ui->setupUi(this);

  // Build a JSON object containing relevant information.
  if (m_details["tomvizVersion"].isNull()) {
    buildJson(m_details);
    buildOpenGL(m_details);
    buildPython(m_details);
  }

  // Populate the tree widget with this information, provide friendly names.
  QTreeWidget* tree = m_ui->information;
  AddItem(tree, "Tomviz Version", m_details["tomvizVersion"].toString());
  AddItem(tree, "ParaView Version", m_details["paraviewVersion"].toString());
  AddItem(tree, "Qt Version", m_details["qtVersion"].toString());
  AddItem(tree, "Python Version", m_details["pythonVersion"].toString());

  AddItem(tree, "OpenGL Vendor", m_details["openglVendor"].toString());
  AddItem(tree, "OpenGL Version", m_details["openglVersion"].toString());
  AddItem(tree, "OpenGL Renderer", m_details["openglRenderer"].toString());
  AddItem(tree, "GLSL Version", m_details["openglShaderVersion"].toString());

  AddItem(tree, "NumPy Version", m_details["numpyVersion"].toString());
  AddItem(tree, "SciPy Version", m_details["scipyVersion"].toString());
  AddItem(tree, "Python Prefix", m_details["pythonPrefix"].toString());
  AddItem(tree, "NumPy Path", m_details["numpyPath"].toString());
  AddItem(tree, "SciPy Path", m_details["scipyPath"].toString());

  tree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

AboutDialog::~AboutDialog() = default;
} // namespace tomviz

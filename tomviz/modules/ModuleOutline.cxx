/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleOutline.h"

#include "DataSource.h"
#include "Utilities.h"
#include "pqProxiesWidget.h"
#include "vtkNew.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"
#include <pqColorChooserButton.h>
#include <vtkGridAxesActor3D.h>
#include <vtkGridAxesHelper.h>
#include <vtkPVRenderView.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace tomviz {

ModuleOutline::ModuleOutline(QObject* parentObject) : Module(parentObject) {}

ModuleOutline::~ModuleOutline()
{
  finalize();
}

QIcon ModuleOutline::icon() const
{
  return QIcon(":/pqWidgets/Icons/pqProbeLocation.svg");
}

bool ModuleOutline::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  auto pxm = data->proxy()->GetSessionProxyManager();

  // Create the outline filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "OutlineFilter"));

  m_outlineFilter = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_outlineFilter);
  controller->PreInitializeProxy(m_outlineFilter);
  vtkSMPropertyHelper(m_outlineFilter, "Input").Set(data->proxy());
  controller->PostInitializeProxy(m_outlineFilter);
  controller->RegisterPipelineProxy(m_outlineFilter);

  // Create the representation for it.
  m_outlineRepresentation = controller->Show(m_outlineFilter, 0, vtkView);
  vtkSMPropertyHelper(m_outlineRepresentation, "Position")
    .Set(data->displayPosition(), 3);
  vtkSMPropertyHelper(m_outlineRepresentation, "Orientation")
    .Set(data->displayOrientation(), 3);
  Q_ASSERT(m_outlineRepresentation);
  // vtkSMPropertyHelper(OutlineRepresentation,
  //                    "Representation").Set("Outline");
  m_outlineRepresentation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  // Init the grid axes
  initializeGridAxes(data, vtkView);
  updateGridAxesColor(offWhite);

  return true;
}

bool ModuleOutline::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_outlineRepresentation);
  controller->UnRegisterProxy(m_outlineFilter);

  if (m_view) {
    m_view->GetRenderer()->RemoveActor(m_gridAxes);
  }

  m_outlineFilter = nullptr;
  m_outlineRepresentation = nullptr;
  return true;
}

QJsonObject ModuleOutline::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["gridVisibility"] = m_gridAxes->GetVisibility() > 0;
  props["gridLines"] = m_gridAxes->GetGenerateGrid();

  QJsonArray color;
  double rgb[3];
  m_gridAxes->GetProperty()->GetDiffuseColor(rgb);
  color << rgb[0] << rgb[1] << rgb[2];
  props["gridColor"] = color;

  props["useCustomAxesTitles"] = m_useCustomAxesTitles;
  props["customXTitle"] = m_customXTitle;
  props["customYTitle"] = m_customYTitle;
  props["customZTitle"] = m_customZTitle;

  json["properties"] = props;
  return json;
}

bool ModuleOutline::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    m_gridAxes->SetVisibility(props["gridVisibility"].toBool() ? 1 : 0);
    m_gridAxes->SetGenerateGrid(props["gridLines"].toBool());
    auto color = props["gridColor"].toArray();
    double rgb[3] = { color[0].toDouble(), color[1].toDouble(),
                      color[2].toDouble() };
    updateGridAxesColor(rgb);

    m_useCustomAxesTitles = props["useCustomAxesTitles"].toBool();
    if (props.contains("customXTitle")) {
      m_customXTitle = props["customXTitle"].toString();
    }
    if (props.contains("customYTitle")) {
      m_customYTitle = props["customYTitle"].toString();
    }
    if (props.contains("customZTitle")) {
      m_customZTitle = props["customZTitle"].toString();
    }
    updateGridAxesTitles();
    return true;
  }
  return false;
}

bool ModuleOutline::setVisibility(bool val)
{
  Q_ASSERT(m_outlineRepresentation);
  vtkSMPropertyHelper(m_outlineRepresentation, "Visibility").Set(val ? 1 : 0);
  m_outlineRepresentation->UpdateVTKObjects();
  if (!val || m_axesVisibility) {
    m_gridAxes->SetVisibility(val ? 1 : 0);
  }
  Module::setVisibility(val);
  return true;
}

bool ModuleOutline::visibility() const
{
  if (m_outlineRepresentation) {
    return vtkSMPropertyHelper(m_outlineRepresentation, "Visibility")
             .GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleOutline::addToPanel(QWidget* panel)
{
  Q_ASSERT(panel && m_outlineRepresentation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QHBoxLayout* layout = new QHBoxLayout;
  QLabel* label = new QLabel("Color");
  layout->addWidget(label);
  layout->addStretch();
  pqColorChooserButton* colorSelector = new pqColorChooserButton(panel);
  colorSelector->setShowAlphaChannel(false);
  layout->addWidget(colorSelector);

  // Show Grid?
  QHBoxLayout* showGridLayout = new QHBoxLayout;
  QCheckBox* showGrid = new QCheckBox(QString("Show Grid"));
  showGrid->setChecked(m_gridAxes->GetGenerateGrid());

  connect(showGrid, &QCheckBox::stateChanged, this, [this](int state) {
    m_gridAxes->SetGenerateGrid(state == Qt::Checked);
    emit renderNeeded();
  });

  showGridLayout->addWidget(showGrid);

  // Custom axes titles
  auto* customAxesTitlesGroupBox = new QGroupBox(panel);
  customAxesTitlesGroupBox->setVisible(m_useCustomAxesTitles);
  auto* customAxesTitlesLayout = new QVBoxLayout(customAxesTitlesGroupBox);

  auto* customXTitleLayout = new QHBoxLayout;
  auto* customYTitleLayout = new QHBoxLayout;
  auto* customZTitleLayout = new QHBoxLayout;
  customAxesTitlesLayout->addLayout(customXTitleLayout);
  customAxesTitlesLayout->addLayout(customYTitleLayout);
  customAxesTitlesLayout->addLayout(customZTitleLayout);

  customXTitleLayout->addWidget(new QLabel("X:"));
  customYTitleLayout->addWidget(new QLabel("Y:"));
  customZTitleLayout->addWidget(new QLabel("Z:"));

  auto* customXTitleEditor = new QLineEdit(m_customXTitle);
  auto* customYTitleEditor = new QLineEdit(m_customYTitle);
  auto* customZTitleEditor = new QLineEdit(m_customZTitle);

  customXTitleLayout->addWidget(customXTitleEditor);
  customYTitleLayout->addWidget(customYTitleEditor);
  customZTitleLayout->addWidget(customZTitleEditor);

  connect(customXTitleEditor, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            m_customXTitle = text;
            updateGridAxesTitles();
            emit renderNeeded();
          });

  connect(customYTitleEditor, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            m_customYTitle = text;
            updateGridAxesTitles();
            emit renderNeeded();
          });

  connect(customZTitleEditor, &QLineEdit::textChanged, this,
          [this](const QString& text) {
            m_customZTitle = text;
            updateGridAxesTitles();
            emit renderNeeded();
          });

  // Use custom axes titles?
  auto* useCustomAxesTitlesLayout = new QHBoxLayout;
  auto* useCustomAxesTitles = new QCheckBox("Custom Axes Titles");
  useCustomAxesTitles->setChecked(m_useCustomAxesTitles);
  useCustomAxesTitlesLayout->addWidget(useCustomAxesTitles);
  connect(useCustomAxesTitles, &QCheckBox::toggled, this,
          [this, customAxesTitlesGroupBox](bool b) {
            m_useCustomAxesTitles = b;
            customAxesTitlesGroupBox->setVisible(b);
            updateGridAxesTitles();
            emit renderNeeded();
          });

  // Show Axes?
  QHBoxLayout* showAxesLayout = new QHBoxLayout;
  QCheckBox* showAxes = new QCheckBox(QString("Show Axes"));
  showAxes->setChecked(m_gridAxes->GetVisibility());
  // Disable "Show Grid" if axes not enabled
  if (!showAxes->isChecked()) {
    showGrid->setEnabled(false);
    useCustomAxesTitles->setEnabled(false);
    customAxesTitlesGroupBox->setVisible(false);
  }
  connect(showAxes, &QCheckBox::stateChanged, this,
          [this, showGrid, useCustomAxesTitles](int state) {
            m_gridAxes->SetVisibility(state == Qt::Checked);
            m_axesVisibility = state == Qt::Checked;
            // Uncheck "Show Grid" and disable it
            if (state == Qt::Unchecked) {
              showGrid->setChecked(false);
              useCustomAxesTitles->setChecked(false);
            }
            showGrid->setEnabled(state == Qt::Checked);
            useCustomAxesTitles->setEnabled(state == Qt::Checked);

            emit renderNeeded();
          });
  showAxesLayout->addWidget(showAxes);

  QVBoxLayout* panelLayout = new QVBoxLayout;
  panelLayout->addItem(layout);
  panelLayout->addItem(showAxesLayout);
  panelLayout->addItem(showGridLayout);
  panelLayout->addItem(useCustomAxesTitlesLayout);
  panelLayout->addWidget(customAxesTitlesGroupBox);
  panelLayout->addStretch();
  panel->setLayout(panelLayout);

  m_links.addPropertyLink(colorSelector, "chosenColorRgbF",
                          SIGNAL(chosenColorChanged(const QColor&)),
                          m_outlineRepresentation,
                          m_outlineRepresentation->GetProperty("DiffuseColor"));

  connect(colorSelector, &pqColorChooserButton::chosenColorChanged,
          [this](const QColor& color) {
            double rgb[3];
            rgb[0] = color.redF();
            rgb[1] = color.greenF();
            rgb[2] = color.blueF();
            updateGridAxesColor(rgb);
          });
  connect(colorSelector, &pqColorChooserButton::chosenColorChanged, this,
          &ModuleOutline::dataUpdated);
}

void ModuleOutline::dataUpdated()
{
  m_links.accept();
  emit renderNeeded();
}

void ModuleOutline::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_outlineRepresentation, "Position").Set(pos, 3);
  m_outlineRepresentation->UpdateVTKObjects();
  m_gridAxes->SetPosition(newX, newY, newZ);
}

void ModuleOutline::dataSourceRotated(double newX, double newY, double newZ)
{
  double orientation[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_outlineRepresentation, "Orientation")
    .Set(orientation, 3);
  m_outlineRepresentation->UpdateVTKObjects();
  m_gridAxes->SetOrientation(newX, newY, newZ);
}

void ModuleOutline::updateGridAxesBounds(DataSource* dataSource)
{
  Q_ASSERT(dataSource);
  double bounds[6];
  dataSource->getBounds(bounds);
  m_gridAxes->SetGridBounds(bounds);
}

void ModuleOutline::initializeGridAxes(DataSource* data,
                                       vtkSMViewProxy* vtkView)
{
  updateGridAxesBounds(data);
  m_gridAxes->SetVisibility(0);
  m_gridAxes->SetGenerateGrid(false);

  // Work around a bug in vtkGridAxesActor3D. GetProperty() returns the
  // vtkProperty associated with a single face, so to get a property associated
  // with all the faces, we need to create a new one and set it.
  vtkNew<vtkProperty> prop;
  prop->DeepCopy(m_gridAxes->GetProperty());
  m_gridAxes->SetProperty(prop);

  // Set mask to show labels on all axes
  m_gridAxes->SetLabelMask(vtkGridAxesHelper::LabelMasks::MIN_X |
                           vtkGridAxesHelper::LabelMasks::MIN_Y |
                           vtkGridAxesHelper::LabelMasks::MIN_Z |
                           vtkGridAxesHelper::LabelMasks::MAX_X |
                           vtkGridAxesHelper::LabelMasks::MAX_Y |
                           vtkGridAxesHelper::LabelMasks::MAX_Z);

  // Set mask to render all faces
  m_gridAxes->SetFaceMask(vtkGridAxesHelper::Faces::MAX_XY |
                          vtkGridAxesHelper::Faces::MAX_YZ |
                          vtkGridAxesHelper::Faces::MAX_ZX |
                          vtkGridAxesHelper::Faces::MIN_XY |
                          vtkGridAxesHelper::Faces::MIN_YZ |
                          vtkGridAxesHelper::Faces::MIN_ZX);

  // Enable front face culling
  prop->SetFrontfaceCulling(1);

  // Disable back face culling
  prop->SetBackfaceCulling(0);

  // Set the titles
  updateGridAxesTitles();

  m_view = vtkPVRenderView::SafeDownCast(vtkView->GetClientSideView());
  m_view->GetRenderer()->AddActor(m_gridAxes);

  connect(data, &DataSource::dataPropertiesChanged, this, [this]() {
    auto dataSource = qobject_cast<DataSource*>(sender());
    updateGridAxesBounds(dataSource);
    updateGridAxesTitles();
    dataSource->proxy()->MarkModified(nullptr);
    dataSource->proxy()->UpdatePipeline();
    emit renderNeeded();
  });
}

void ModuleOutline::updateGridAxesColor(double* color)
{
  for (int i = 0; i < 6; i++) {
    vtkNew<vtkTextProperty> prop;
    prop->SetColor(color);
    m_gridAxes->SetTitleTextProperty(i, prop);
    m_gridAxes->SetLabelTextProperty(i, prop);
  }
  m_gridAxes->GetProperty()->SetDiffuseColor(color);
  vtkSMPropertyHelper(m_outlineRepresentation, "DiffuseColor").Set(color, 3);
  m_outlineRepresentation->UpdateVTKObjects();
}

void ModuleOutline::updateGridAxesTitles()
{
  QString xTitle, yTitle, zTitle;
  if (m_useCustomAxesTitles) {
    xTitle = m_customXTitle;
    yTitle = m_customYTitle;
    zTitle = m_customZTitle;
  } else {
    auto units = dataSource()->getUnits();
    xTitle = QString("X (%1)").arg(units);
    yTitle = QString("Y (%1)").arg(units);
    zTitle = QString("Z (%1)").arg(units);
  }

  m_gridAxes->SetXTitle(xTitle.toUtf8().data());
  m_gridAxes->SetYTitle(yTitle.toUtf8().data());
  m_gridAxes->SetZTitle(zTitle.toUtf8().data());
}

} // end of namespace tomviz

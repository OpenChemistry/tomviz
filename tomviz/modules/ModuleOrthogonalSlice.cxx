/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleOrthogonalSlice.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "IntSliderWidget.h"
#include "ScalarsComboBox.h"
#include "Utilities.h"
#include "pqPropertyLinks.h"
#include "pqSignalAdaptors.h"
#include "pqWidgetRangeDomain.h"
#include "vtkAlgorithm.h"
#include "vtkImageData.h"
#include "vtkImageReslice.h"
#include "vtkNew.h"
#include "vtkSMPVRepresentationProxy.h"
#include "vtkSMParaViewPipelineControllerWithRendering.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMViewProxy.h"
#include "vtkSmartPointer.h"
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkPointData.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
namespace tomviz {

ModuleOrthogonalSlice::ModuleOrthogonalSlice(QObject* parentObject)
  : Module(parentObject)
{}

ModuleOrthogonalSlice::~ModuleOrthogonalSlice()
{
  finalize();
}

QIcon ModuleOrthogonalSlice::icon() const
{
  return QIcon(":/icons/orthoslice.png");
}

bool ModuleOrthogonalSlice::initialize(DataSource* data,
                                       vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;

  auto pxm = data->proxy()->GetSessionProxyManager();

  // Create the pass through filter.
  vtkSmartPointer<vtkSMProxy> proxy;
  proxy.TakeReference(pxm->NewProxy("filters", "PassThrough"));

  m_passThrough = vtkSMSourceProxy::SafeDownCast(proxy);
  Q_ASSERT(m_passThrough);
  controller->PreInitializeProxy(m_passThrough);
  vtkSMPropertyHelper(m_passThrough, "Input").Set(data->proxy());
  controller->PostInitializeProxy(m_passThrough);
  controller->RegisterPipelineProxy(m_passThrough);

  // Create the representation for it.
  m_representation = controller->Show(m_passThrough, 0, vtkView);
  Q_ASSERT(m_representation);

  vtkSMRepresentationProxy::SetRepresentationType(m_representation, "Slice");
  vtkSMPropertyHelper(m_representation, "Position")
    .Set(data->displayPosition(), 3);

  // pick proper color/opacity maps.
  updateColorMap();
  m_representation->UpdateVTKObjects();

  // Give the proxy a friendly name for the GUI/Python world.
  if (auto p = convert<pqProxy*>(proxy)) {
    p->rename(label());
  }

  connect(data, SIGNAL(activeScalarsChanged()), SLOT(onScalarArrayChanged()));

  onScalarArrayChanged();

  return true;
}

void ModuleOrthogonalSlice::updateColorMap()
{
  Q_ASSERT(m_representation);

  vtkSMPropertyHelper(m_representation, "LookupTable").Set(colorMap());
  vtkSMPropertyHelper(m_representation, "ScalarOpacityFunction")
    .Set(opacityMap());
  m_representation->UpdateVTKObjects();
}

bool ModuleOrthogonalSlice::finalize()
{
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> controller;
  controller->UnRegisterProxy(m_representation);
  controller->UnRegisterProxy(m_passThrough);

  m_passThrough = nullptr;
  m_representation = nullptr;
  return true;
}

bool ModuleOrthogonalSlice::setVisibility(bool val)
{
  Q_ASSERT(m_representation);
  vtkSMPropertyHelper(m_representation, "Visibility").Set(val ? 1 : 0);
  m_representation->UpdateVTKObjects();
  return true;
}

bool ModuleOrthogonalSlice::visibility() const
{
  if (m_representation) {
    return vtkSMPropertyHelper(m_representation, "Visibility").GetAsInt() != 0;
  } else {
    return false;
  }
}

void ModuleOrthogonalSlice::addToPanel(QWidget* panel)
{
  Q_ASSERT(m_representation);

  if (panel->layout()) {
    delete panel->layout();
  }

  QFormLayout* layout = new QFormLayout;

  m_opacityCheckBox = new QCheckBox("Map Opacity");
  layout->addRow(m_opacityCheckBox);

  QCheckBox* mapScalarsCheckBox = new QCheckBox("Color Map Data");
  layout->addRow(mapScalarsCheckBox);

  auto line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addRow(line);

  m_scalarsCombo = new ScalarsComboBox();
  m_scalarsCombo->setOptions(dataSource(), this);
  layout->addRow("Scalars", m_scalarsCombo);

  QComboBox* direction = new QComboBox;
  direction->addItem("XY Plane");
  direction->addItem("YZ Plane");
  direction->addItem("XZ Plane");

  layout->addRow("Direction", direction);

  pqSignalAdaptorComboBox* adaptor = new pqSignalAdaptorComboBox(direction);

  IntSliderWidget* sliceIndex = new IntSliderWidget(true);
  sliceIndex->setLineEditWidth(50);
  sliceIndex->setPageStep(1);
  layout->addRow("Slice", sliceIndex);

  DoubleSliderWidget* opacitySlider = new DoubleSliderWidget(true);
  opacitySlider->setLineEditWidth(50);
  layout->addRow("Opacity", opacitySlider);

  panel->setLayout(layout);

  m_links.addPropertyLink(sliceIndex, "value", SIGNAL(valueEdited(int)),
                          m_representation,
                          m_representation->GetProperty("Slice"), 0);
  new pqWidgetRangeDomain(sliceIndex, "minimum", "maximum",
                          m_representation->GetProperty("Slice"), 0);
  m_links.addPropertyLink(opacitySlider, "value", SIGNAL(valueEdited(double)),
                          m_representation,
                          m_representation->GetProperty("Opacity"), 0);
  m_links.addPropertyLink(mapScalarsCheckBox, "checked", SIGNAL(toggled(bool)),
                          m_representation,
                          m_representation->GetProperty("MapScalars"), 0);

  m_links.addPropertyLink(adaptor, "currentText",
                          SIGNAL(currentTextChanged(QString)), m_representation,
                          m_representation->GetProperty("SliceMode"));

  connect(sliceIndex, &IntSliderWidget::valueEdited, this,
          &ModuleOrthogonalSlice::dataUpdated);
  connect(direction, &QComboBox::currentTextChanged, this,
          &ModuleOrthogonalSlice::dataUpdated);
  connect(opacitySlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleOrthogonalSlice::dataUpdated);
  connect(mapScalarsCheckBox, SIGNAL(toggled(bool)), this, SLOT(dataUpdated()));

  connect(m_opacityCheckBox, &QCheckBox::toggled, this, [this](bool val) {
    m_mapOpacity = val;
    // Ensure the colormap is detached before applying opacity
    if (val) {
      setUseDetachedColorMap(val);
    }
    auto func = vtkPVDiscretizableColorTransferFunction::SafeDownCast(
      colorMap()->GetClientSideObject());
    func->SetEnableOpacityMapping(val);
    emit opacityEnforced(val);
    updateColorMap();
    emit renderNeeded();
  });

  connect(m_scalarsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            setActiveScalars(m_scalarsCombo->itemData(idx).toInt());
            onScalarArrayChanged();
          });

  m_opacityCheckBox->setChecked(m_mapOpacity);
}

void ModuleOrthogonalSlice::dataUpdated()
{
  m_links.accept();
  emit renderNeeded();
}

void ModuleOrthogonalSlice::onScalarArrayChanged()
{
  QString arrayName;
  if (activeScalars() == Module::DEFAULT_SCALARS) {
    arrayName = dataSource()->activeScalars();
  } else {
    arrayName = dataSource()->scalarsName(activeScalars());
  }
  vtkSMPropertyHelper(m_representation, "ColorArrayName")
    .SetInputArrayToProcess(vtkDataObject::FIELD_ASSOCIATION_POINTS,
                            arrayName.toLatin1().data());
  m_representation->UpdateVTKObjects();

  emit renderNeeded();
}

QJsonObject ModuleOrthogonalSlice::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  vtkSMPropertyHelper sliceMode(m_representation->GetProperty("SliceMode"));
  props["sliceMode"] = sliceMode.GetAsInt();
  vtkSMPropertyHelper slice(m_representation->GetProperty("Slice"));
  props["slice"] = slice.GetAsInt();
  vtkSMPropertyHelper opacity(m_representation->GetProperty("Opacity"));
  props["opacity"] = opacity.GetAsDouble();
  vtkSMPropertyHelper mapScalars(m_representation->GetProperty("MapScalars"));
  props["mapScalars"] = mapScalars.GetAsInt() != 0;
  props["mapOpacity"] = m_mapOpacity;

  json["properties"] = props;
  return json;
}

bool ModuleOrthogonalSlice::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject() && m_representation) {
    auto rep = m_representation.Get();
    auto props = json["properties"].toObject();
    vtkSMPropertyHelper(rep, "SliceMode").Set(props["sliceMode"].toInt());
    vtkSMPropertyHelper(rep, "Slice").Set(props["slice"].toInt());
    vtkSMPropertyHelper(rep, "Opacity").Set(props["opacity"].toDouble());
    vtkSMPropertyHelper(rep, "MapScalars")
      .Set(props["mapScalars"].toBool() ? 1 : 0);
    if (props.contains("mapOpacity")) {
      m_mapOpacity = props["mapOpacity"].toBool();
      if (m_opacityCheckBox) {
        m_opacityCheckBox->setChecked(m_mapOpacity);
      }
    }
    rep->UpdateVTKObjects();
    m_scalarsCombo->setOptions(dataSource(), this);
    return true;
  }
  return false;
}

void ModuleOrthogonalSlice::dataSourceMoved(double newX, double newY,
                                            double newZ)
{
  double pos[3] = { newX, newY, newZ };
  vtkSMPropertyHelper(m_representation, "Position").Set(pos, 3);
  m_representation->UpdateVTKObjects();
}

vtkSmartPointer<vtkDataObject> ModuleOrthogonalSlice::getDataToExport()
{
  vtkAlgorithm* algorithm =
    vtkAlgorithm::SafeDownCast(m_passThrough->GetClientSideObject());
  vtkNew<vtkImageData> volume;
  volume->ShallowCopy(
    vtkImageData::SafeDownCast(algorithm->GetOutputDataObject(0)));

  double origin[3], spacing[3];
  int extent[6];
  volume->GetOrigin(origin);
  volume->GetSpacing(spacing);
  volume->GetExtent(extent);

  QString arrayName;
  if (activeScalars() == Module::DEFAULT_SCALARS) {
    arrayName = dataSource()->activeScalars();
  } else {
    arrayName = dataSource()->scalarsName(activeScalars());
  }
  volume->GetPointData()->SetActiveScalars(arrayName.toLatin1().data());

  double cosines[9] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  double newOrigin[3] = { origin[0], origin[1], origin[2] };
  vtkSMPropertyHelper directionHelper(m_representation, "SliceMode");
  vtkSMPropertyHelper sliceNumHelper(m_representation, "Slice");
  switch (directionHelper.GetAsInt()) {
    case 5: // XY Plane
      cosines[0] = cosines[4] = cosines[8] = 1;
      newOrigin[2] =
        origin[2] + spacing[2] * (extent[4] + sliceNumHelper.GetAsInt());
      break;
    case 6: // YZ Plane
      cosines[4] = cosines[6] = 1;
      cosines[2] = -1;
      newOrigin[0] =
        origin[0] + spacing[0] * (extent[0] + sliceNumHelper.GetAsInt());
      break;
    case 7: // XZ Plane
      cosines[0] = cosines[7] = 1;
      cosines[5] = -1;
      newOrigin[1] =
        origin[1] + spacing[1] * (extent[2] + sliceNumHelper.GetAsInt());
      break;
  }

  vtkNew<vtkImageReslice> reslice;
  reslice->SetInputData(volume);
  reslice->SetResliceAxesDirectionCosines(cosines);
  reslice->SetResliceAxesOrigin(newOrigin);
  reslice->SetOutputDimensionality(2);
  reslice->Update();

  vtkSmartPointer<vtkImageData> output = reslice->GetOutput();
  return output;
}

//-----------------------------------------------------------------------------
bool ModuleOrthogonalSlice::isProxyPartOfModule(vtkSMProxy* proxy)
{
  return (proxy == m_passThrough.Get()) || (proxy == m_representation.Get());
}

std::string ModuleOrthogonalSlice::getStringForProxy(vtkSMProxy* proxy)
{
  if (proxy == m_passThrough.Get()) {
    return "PassThrough";
  } else if (proxy == m_representation.Get()) {
    return "Representation";
  } else {
    qWarning(
      "Unknown proxy passed to module orthogonal slice in save animation");
    return "";
  }
}

vtkSMProxy* ModuleOrthogonalSlice::getProxyForString(const std::string& str)
{
  if (str == "PassThrough") {
    return m_passThrough.Get();
  } else if (str == "Representation") {
    return m_representation.Get();
  } else {
    return nullptr;
  }
}

bool ModuleOrthogonalSlice::areScalarsMapped() const
{
  vtkSMPropertyHelper mapScalars(m_representation->GetProperty("MapScalars"));
  return mapScalars.GetAsInt() != 0;
}

} // namespace tomviz

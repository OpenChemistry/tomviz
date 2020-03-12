/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ModuleSlice.h"

#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "IntSliderWidget.h"
#include "ScalarsComboBox.h"
#include "Utilities.h"
#include "vtkActiveScalarsProducer.h"

#include <vtkCommand.h>
#include <vtkDataObject.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkNonOrthoImagePlaneWidget.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarsToColors.h>
#include <vtkTrivialProducer.h>

#include <pqCoreUtilities.h>
#include <pqLineEdit.h>
#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace tomviz {

ModuleSlice::ModuleSlice(QObject* parentObject) : Module(parentObject) {}

ModuleSlice::~ModuleSlice()
{
  finalize();
}

QIcon ModuleSlice::icon() const
{
  return QIcon(":/icons/orthoslice.png");
}

bool ModuleSlice::initialize(DataSource* data, vtkSMViewProxy* vtkView)
{
  if (!Module::initialize(data, vtkView)) {
    return false;
  }

  const bool widgetSetup = setupWidget(vtkView);

  if (widgetSetup) {
    m_widget->SetDisplayOffset(data->displayPosition());
    m_widget->On();
    m_widget->InteractionOn();
    onDirectionChanged(m_direction);
    onTextureInterpolateChanged(m_interpolate);
    pqCoreUtilities::connect(m_widget, vtkCommand::InteractionEvent, this,
                             SLOT(onPlaneChanged()));
    connect(data, SIGNAL(dataChanged()), this, SLOT(dataUpdated()));
    connect(data, &DataSource::activeScalarsChanged, this,
            &ModuleSlice::onScalarArrayChanged);
  }

  Q_ASSERT(m_widget);
  return widgetSetup;
}

bool ModuleSlice::setupWidget(vtkSMViewProxy* vtkView)
{
  vtkRenderWindowInteractor* rwi = vtkView->GetRenderWindow()->GetInteractor();

  if (!rwi) {
    return false;
  }

  m_widget = vtkSmartPointer<vtkNonOrthoImagePlaneWidget>::New();
  m_widget->SetVoxelValueFn(
    [this](const vtkVector3i& ijk, double v) { emit mouseOverVoxel(ijk, v); });

  // Set the interactor on the widget to be what the current
  // render window is using.
  m_widget->SetInteractor(rwi);

  // Setup the color of the border of the widget.
  {
    double color[3] = { 1, 0, 0 };
    m_widget->GetPlaneProperty()->SetColor(color);
  }

  // Turn texture interpolation to be linear.
  m_widget->TextureInterpolateOn();
  m_widget->SetResliceInterpolateToLinear();

  // Construct the transfer function proxy for the widget.
  vtkSMProxy* lut = colorMap();

  // Set the widgets lookup table to be the one that the transfer function
  // manager is using.
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  m_widget->SetLookupTable(stc);

  // Lastly we set up the input connection.
  m_producer->SetOutput(dataSource()->producer()->GetOutputDataObject(0));
  m_widget->SetInputConnection(m_producer->GetOutputPort());

  Q_ASSERT(rwi);
  onPlaneChanged();
  return true;
}

void ModuleSlice::updateColorMap()
{
  Q_ASSERT(m_widget);

  // Construct the transfer function proxy for the widget
  vtkSMProxy* lut = colorMap();

  // set the widgets lookup table to be the one that the transfer function
  // manager is using
  vtkScalarsToColors* stc =
    vtkScalarsToColors::SafeDownCast(lut->GetClientSideObject());
  m_widget->SetLookupTable(stc);
}

void ModuleSlice::updateSliceWidget()
{
  if (!m_sliceSlider || !imageData())
    return;

  int dims[3];
  imageData()->GetDimensions(dims);

  int axis = directionAxis(m_direction);
  int maxSlice = dims[axis] - 1;

  m_sliceSlider->setMinimum(0);
  m_sliceSlider->setMaximum(maxSlice);
}

bool ModuleSlice::finalize()
{
  if (m_widget != nullptr) {
    m_widget->Off();
  }

  return true;
}

bool ModuleSlice::setVisibility(bool val)
{
  Q_ASSERT(m_widget);
  m_widget->SetEnabled(val ? 1 : 0);
  Module::setVisibility(val);
  updateInteractionState();

  return true;
}

bool ModuleSlice::visibility() const
{
  return m_widget->GetEnabled() != 0;
}

void ModuleSlice::addToPanel(QWidget* panel)
{
  if (panel->layout()) {
    delete panel->layout();
  }

  QVBoxLayout* layout = new QVBoxLayout;
  QFormLayout* formLayout = new QFormLayout;

  QWidget* container = new QWidget;
  container->setLayout(formLayout);
  layout->addWidget(container);
  formLayout->setContentsMargins(0, 0, 0, 0);

  m_opacityCheckBox = new QCheckBox("Map Opacity");
  formLayout->addRow(m_opacityCheckBox);

  m_mapScalarsCheckBox = new QCheckBox("Color Map Data");
  m_mapScalarsCheckBox->setChecked(areScalarsMapped());
  formLayout->addRow(m_mapScalarsCheckBox);
  connect(m_mapScalarsCheckBox, &QCheckBox::toggled, this,
          &ModuleSlice::setMapScalars);

  auto line = new QFrame;
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  formLayout->addRow(line);

  m_scalarsCombo = new ScalarsComboBox();
  m_scalarsCombo->setOptions(dataSource(), this);
  formLayout->addRow("Scalars", m_scalarsCombo);

  m_directionCombo = new QComboBox();
  m_directionCombo->addItem("XY Plane", QVariant(Direction::XY));
  m_directionCombo->addItem("YZ Plane", QVariant(Direction::YZ));
  m_directionCombo->addItem("XZ Plane", QVariant(Direction::XZ));
  m_directionCombo->addItem("Custom", QVariant(Direction::Custom));
  m_directionCombo->setCurrentIndex(static_cast<int>(m_direction));
  formLayout->addRow("Direction", m_directionCombo);

  m_sliceSlider = new IntSliderWidget(true);
  m_sliceSlider->setLineEditWidth(50);
  m_sliceSlider->setPageStep(1);
  m_sliceSlider->setMinimum(0);
  int axis = directionAxis(m_direction);
  bool isOrtho = axis >= 0;
  if (isOrtho) {
    int dims[3];
    imageData()->GetDimensions(dims);
    m_sliceSlider->setMaximum(dims[axis] - 1);
  }

  // Sanity check: make sure the slice value is within the bounds
  if (m_slice < m_sliceSlider->minimum())
    m_slice = m_sliceSlider->minimum();
  else if (m_slice > m_sliceSlider->maximum())
    m_slice = m_sliceSlider->maximum();

  m_sliceSlider->setValue(m_slice);

  formLayout->addRow("Slice", m_sliceSlider);

  m_thicknessSpin = new QSpinBox();
  m_thicknessSpin->setMaximum(m_sliceSlider->maximum());
  m_thicknessSpin->setMinimum(1);
  m_thicknessSpin->setSingleStep(2);
  m_thicknessSpin->setValue(m_sliceThickness);
  formLayout->addRow("Slice Thickness", m_thicknessSpin);

  m_sliceCombo = new QComboBox();
  m_sliceCombo->addItem("Minimum", QVariant(Mode::Min));
  m_sliceCombo->addItem("Maximum", QVariant(Mode::Max));
  m_sliceCombo->addItem("Mean", QVariant(Mode::Mean));
  m_sliceCombo->addItem("Summation", QVariant(Mode::Sum));
  m_sliceCombo->setCurrentIndex(static_cast<int>(m_thickSliceMode));
  formLayout->addRow("Aggregation", m_sliceCombo);

  m_opacitySlider = new DoubleSliderWidget(true);
  m_opacitySlider->setLineEditWidth(50);
  m_opacitySlider->setMinimum(0);
  m_opacitySlider->setMaximum(1);
  m_opacitySlider->setValue(m_opacity);
  formLayout->addRow("Opacity", m_opacitySlider);

  m_interpolateCheckBox = new QCheckBox("Interpolate Texture");
  m_interpolateCheckBox->setChecked(m_interpolate);
  formLayout->addRow(m_interpolateCheckBox);

  m_showArrowCheckBox = new QCheckBox("Show Arrow");
  m_showArrowCheckBox->setChecked(showArrow());
  formLayout->addRow(m_showArrowCheckBox);
  connect(m_showArrowCheckBox, &QCheckBox::toggled, this,
          &ModuleSlice::setShowArrow);

  QLabel* label = new QLabel("Point on Plane");
  layout->addWidget(label);
  QHBoxLayout* row = new QHBoxLayout;
  const char* labels[] = { "X:", "Y:", "Z:" };
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setEnabled(!isOrtho);
    inputBox->setValidator(new QDoubleValidator(inputBox));
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleSlice::updatePointOnPlane);
    row->addWidget(inputBox);
    m_pointInputs[i] = inputBox;
  }
  layout->addItem(row);

  label = new QLabel("Plane Normal");
  layout->addWidget(label);
  row = new QHBoxLayout;
  for (int i = 0; i < 3; ++i) {
    label = new QLabel(labels[i]);
    row->addWidget(label);
    pqLineEdit* inputBox = new pqLineEdit;
    inputBox->setEnabled(!isOrtho);
    inputBox->setValidator(new QDoubleValidator(inputBox));
    connect(inputBox, &pqLineEdit::textChangedAndEditingFinished, this,
            &ModuleSlice::updatePlaneNormal);
    row->addWidget(inputBox);
    m_normalInputs[i] = inputBox;
  }
  layout->addItem(row);

  // Update the Qt widget values
  onPlaneChanged();

  layout->addStretch();

  panel->setLayout(layout);

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

  m_opacityCheckBox->setChecked(m_mapOpacity);

  connect(m_scalarsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            setActiveScalars(m_scalarsCombo->itemData(idx).toInt());
            onScalarArrayChanged();
          });

  connect(m_directionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int idx) {
            Direction dir = m_directionCombo->itemData(idx).value<Direction>();
            onDirectionChanged(dir);
          });

  connect(m_interpolateCheckBox, &QCheckBox::toggled, this,
          &ModuleSlice::onTextureInterpolateChanged);

  connect(m_sliceSlider, &IntSliderWidget::valueEdited, this,
          QOverload<int>::of(&ModuleSlice::onSliceChanged));
  connect(m_sliceSlider, &IntSliderWidget::valueChanged, this,
          QOverload<int>::of(&ModuleSlice::onSliceChanged));

  connect(m_thicknessSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &ModuleSlice::onThicknessChanged);
  connect(m_sliceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ModuleSlice::onThickSliceModeChanged);

  connect(m_opacitySlider, &DoubleSliderWidget::valueEdited, this,
          &ModuleSlice::onOpacityChanged);
  connect(m_opacitySlider, &DoubleSliderWidget::valueChanged, this,
          &ModuleSlice::onOpacityChanged);
}

void ModuleSlice::dataUpdated()
{
  // In case there are new slices, update min and max
  updateSliceWidget();
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

void ModuleSlice::setMapScalars(bool b)
{
  if (b != areScalarsMapped()) {
    m_widget->SetMapScalars(b);
    emit renderNeeded();
  }
}

void ModuleSlice::setShowArrow(bool b)
{
  if (b != showArrow()) {
    m_widget->SetArrowVisibility(b);
    updateInteractionState();
    emit renderNeeded();
  }
}

bool ModuleSlice::showArrow() const
{
  return m_widget->GetArrowVisibility() != 0;
}

void ModuleSlice::updateInteractionState()
{
  // We can only update the interaction if the widget is visible
  if (visibility())
    m_widget->SetInteraction(showArrow());
}

void ModuleSlice::updatePointOnPlane()
{
  double point[3] = { 0, 0, 0 };

  for (int i = 0; i < 3; ++i) {
    const auto input = m_pointInputs[i];
    if (input->text().isEmpty())
      continue;

    point[i] = input->text().toDouble();
  }
  m_widget->SetCenter(point);
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

void ModuleSlice::updatePlaneNormal()
{
  double normal[3] = { 0, 0, 0 };

  for (int i = 0; i < 3; ++i) {
    const auto input = m_normalInputs[i];
    if (input->text().isEmpty())
      continue;

    normal[i] = input->text().toDouble();
  }
  m_widget->SetNormal(normal);
  m_widget->UpdatePlacement();
  emit renderNeeded();
}

QJsonObject ModuleSlice::serialize() const
{
  auto json = Module::serialize();
  auto props = json["properties"].toObject();

  props["showArrow"] = showArrow();

  // Serialize the plane
  double point[3];
  m_widget->GetOrigin(point);
  QJsonArray origin = { point[0], point[1], point[2] };
  m_widget->GetPoint1(point);
  QJsonArray point1 = { point[0], point[1], point[2] };
  m_widget->GetPoint2(point);
  QJsonArray point2 = { point[0], point[1], point[2] };

  props["origin"] = origin;
  props["point1"] = point1;
  props["point2"] = point2;
  props["mapScalars"] = areScalarsMapped();
  props["mapOpacity"] = m_mapOpacity;

  props["slice"] = m_slice;
  props["sliceThickness"] = m_sliceThickness;
  props["thickSliceMode"] = m_thickSliceMode;
  QVariant qData;
  qData.setValue(m_direction);
  props["direction"] = qData.toString();
  props["interpolate"] = m_interpolate;
  props["opacity"] = m_opacity;

  json["properties"] = props;
  return json;
}

bool ModuleSlice::deserialize(const QJsonObject& json)
{
  if (!Module::deserialize(json)) {
    return false;
  }
  if (json["properties"].isObject()) {
    auto props = json["properties"].toObject();
    setShowArrow(props["showArrow"].toBool());
    if (m_showArrowCheckBox) {
      m_showArrowCheckBox->setChecked(showArrow());
    }
    if (props.contains("origin") && props.contains("point1") &&
        props.contains("point2")) {
      auto o = props["origin"].toArray();
      auto p1 = props["point1"].toArray();
      auto p2 = props["point2"].toArray();
      double origin[3] = { o[0].toDouble(), o[1].toDouble(), o[2].toDouble() };
      double point1[3] = { p1[0].toDouble(), p1[1].toDouble(),
                           p1[2].toDouble() };
      double point2[3] = { p2[0].toDouble(), p2[1].toDouble(),
                           p2[2].toDouble() };
      m_widget->SetOrigin(origin);
      m_widget->SetPoint1(point1);
      m_widget->SetPoint2(point2);
    }
    setMapScalars(props["mapScalars"].toBool());
    if (m_mapScalarsCheckBox) {
      m_mapScalarsCheckBox->setChecked(areScalarsMapped());
    }
    if (props.contains("mapOpacity")) {
      m_mapOpacity = props["mapOpacity"].toBool();
      if (m_opacityCheckBox) {
        m_opacityCheckBox->setChecked(m_mapOpacity);
      }
    }
    m_widget->UpdatePlacement();
    m_scalarsCombo->setOptions(dataSource(), this);
    // If deserializing a former OrthogonalSlice, the direction is encoded in
    // the property "sliceMode" as an int
    if (props.contains("sliceMode")) {
      Direction direction = modeToDirection(props["sliceMode"].toInt());
      onDirectionChanged(direction);
    }
    if (props.contains("sliceThickness")) {
      m_sliceThickness = props["sliceThickness"].toInt();
      onThicknessChanged(m_sliceThickness);
    }
    if (props.contains("thickSliceMode")) {
      int mode = props["thickSliceMode"].toInt();
      onThickSliceModeChanged(mode);
    }
    if (props.contains("direction")) {
      Direction direction = stringToDirection(props["direction"].toString());
      onDirectionChanged(direction);
    }
    if (props.contains("slice")) {
      m_slice = props["slice"].toInt();
      onSliceChanged(m_slice);
    }
    if (props.contains("opacity")) {
      m_opacity = props["opacity"].toDouble();
      onOpacityChanged(m_opacity);
      if (m_opacitySlider) {
        m_opacitySlider->setValue(m_opacity);
      }
    }
    if (props.contains("interpolate")) {
      m_interpolate = props["interpolate"].toBool();
      onTextureInterpolateChanged(m_interpolate);
      if (m_interpolateCheckBox) {
        m_interpolateCheckBox->setChecked(m_interpolate);
      }
    }
    onPlaneChanged();
    return true;
  }
  return false;
}

void ModuleSlice::onPlaneChanged()
{
  // Avoid recursive clobbering of the plane position
  if (m_ignoreSignals) {
    return;
  }
  m_ignoreSignals = true;

  double* centerPoint = m_widget->GetCenter();
  double* normalVector = m_widget->GetNormal();
  for (int i = 0; i < 3; ++i) {
    if (m_pointInputs[i]) {
      QSignalBlocker b(m_pointInputs[i]);
      m_pointInputs[i]->setText(QString::number(centerPoint[i]));
    }
    if (m_normalInputs[i]) {
      QSignalBlocker b(m_normalInputs[i]);
      m_normalInputs[i]->setText(QString::number(normalVector[i]));
    }
  }

  // Adjust the slice slider if the slice has changed from dragging the arrow
  onSliceChanged(centerPoint);

  m_ignoreSignals = false;
}

void ModuleSlice::dataSourceMoved(double newX, double newY, double newZ)
{
  double pos[3] = { newX, newY, newZ };
  m_widget->SetDisplayOffset(pos);
}

vtkDataObject* ModuleSlice::dataToExport()
{
  return m_widget->GetResliceOutput();
}

bool ModuleSlice::areScalarsMapped() const
{
  return m_widget->GetMapScalars() != 0;
}

vtkImageData* ModuleSlice::imageData() const
{
  vtkImageData* data = vtkImageData::SafeDownCast(
    dataSource()->producer()->GetOutputDataObject(0));
  Q_ASSERT(data);
  return data;
}

void ModuleSlice::onScalarArrayChanged()
{
  QString arrayName;
  if (activeScalars() == Module::DEFAULT_SCALARS) {
    arrayName = dataSource()->activeScalars();
  } else {
    arrayName = dataSource()->scalarsName(activeScalars());
  }
  m_producer->SetActiveScalars(arrayName.toLatin1().data());
  emit renderNeeded();
}

void ModuleSlice::onDirectionChanged(Direction direction)
{
  m_direction = direction;
  int axis = directionAxis(direction);

  bool isOrtho = axis >= 0;

  for (int i = 0; i < 3; ++i) {
    if (m_pointInputs[i]) {
      m_pointInputs[i]->setEnabled(!isOrtho);
    }
    if (m_normalInputs[i]) {
      m_normalInputs[i]->setEnabled(!isOrtho);
    }
  }
  if (m_sliceSlider) {
    m_sliceSlider->setVisible(isOrtho);
  }

  m_widget->SetPlaneOrientation(axis);

  if (m_directionCombo) {
    if (direction != m_directionCombo->currentData().value<Direction>()) {
      for (int i = 0; i < m_directionCombo->count(); ++i) {
        Direction data = m_directionCombo->itemData(i).value<Direction>();
        if (data == direction) {
          m_directionCombo->setCurrentIndex(i);
        }
      }
    }
  }

  if (!isOrtho) {
    return;
  }

  int dims[3];
  imageData()->GetDimensions(dims);

  double normal[3] = { 0, 0, 0 };
  int slice = 0;
  int maxSlice = 0;

  normal[axis] = 1;
  slice = dims[axis] / 2;
  maxSlice = dims[axis] - 1;

  m_widget->SetNormal(normal);
  if (m_sliceSlider) {
    m_sliceSlider->setMinimum(0);
    m_sliceSlider->setMaximum(maxSlice);
  }
  onSliceChanged(slice);
  onPlaneChanged();
  dataUpdated();
}

void ModuleSlice::onSliceChanged(int slice)
{
  m_slice = slice;
  int axis = directionAxis(m_direction);

  if (axis < 0) {
    return;
  }

  m_widget->SetSliceIndex(slice);
  if (m_sliceSlider) {
    m_sliceSlider->setValue(slice);
  }
  onPlaneChanged();
  dataUpdated();
}

void ModuleSlice::onSliceChanged(double* point)
{
  int axis = directionAxis(m_direction);
  if (axis < 0) {
    return;
  }

  int dims[3];
  imageData()->GetDimensions(dims);
  double bounds[6];
  imageData()->GetBounds(bounds);

  // Due to changes from commit 43182619 the point on the slice plane could
  // fall outside the bounds of the volume.
  // This could yield slice numbers that are negative, or are larger than
  // the number of slices. The next two lines ensure this never happens.
  point[axis] = std::max(point[axis], bounds[2 * axis]);
  point[axis] = std::min(point[axis], bounds[2 * axis + 1]);

  double slice = (dims[axis] - 1) * (point[axis] - bounds[2 * axis]) /
          (bounds[2 * axis + 1] - bounds[2 * axis]);

  onSliceChanged(round(slice));
}

void ModuleSlice::onTextureInterpolateChanged(bool flag)
{
  m_interpolate = flag;
  if (!m_widget) {
    return;
  }
  int val = flag ? 1 : 0;
  m_widget->SetTextureInterpolate(val);
  m_widget->SetResliceInterpolate(val);
  emit renderNeeded();
}

void ModuleSlice::onOpacityChanged(double opacity)
{
  m_opacity = opacity;
  m_widget->SetOpacity(opacity);
  emit renderNeeded();
}

void ModuleSlice::onThicknessChanged(int value)
{
  m_sliceThickness = value;
  if(m_thicknessSpin) {
    m_thicknessSpin->setValue(value);
  }
  m_widget->SetSliceThickness(value);
  emit renderNeeded();
}

void ModuleSlice::onThickSliceModeChanged(int index)
{
  m_thickSliceMode = static_cast<Mode>(index);
  if (m_sliceCombo) {
    m_sliceCombo->setCurrentIndex(index);
  }
  m_widget->SetThickSliceMode(index);
  emit renderNeeded();
}

int ModuleSlice::directionAxis(Direction direction)
{
  switch (direction) {
    case Direction::XY: {
      return 2;
    }
    case Direction::YZ: {
      return 0;
    }
    case Direction::XZ: {
      return 1;
    }
    default: {
      return -1;
    }
  }
}

ModuleSlice::Direction ModuleSlice::stringToDirection(const QString& name)
{
  if (name == "XY") {
    return Direction::XY;
  } else if (name == "YZ") {
    return Direction::YZ;
  } else if (name == "XZ") {
    return Direction::XZ;
  } else {
    return Direction::Custom;
  }
}

ModuleSlice::Direction ModuleSlice::modeToDirection(int sliceMode)
{
  switch (sliceMode) {
    case 5: {
      return Direction::XY;
    }
    case 6: {
      return Direction::YZ;
    }
    case 7: {
      return Direction::XZ;
    }
    default: {
      return Direction::Custom;
    }
  }
}

} // namespace tomviz

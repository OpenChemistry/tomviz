/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "VolumePropertiesWidget.h"

#include "ActiveObjects.h"
#include "InteractiveTransformWidget.h"
#include "OutputPort.h"
#include "PortData.h"
#include "VolumeScalarsModel.h"
#include "data/VolumeData.h"

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkPointData.h>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace tomviz {
namespace pipeline {

namespace {

QString dataTypeName(int vtkType)
{
  switch (vtkType) {
    case VTK_VOID:
      return "void";
    case VTK_BIT:
      return "bit";
    case VTK_CHAR:
      return "char";
    case VTK_SIGNED_CHAR:
      return "signed char";
    case VTK_UNSIGNED_CHAR:
      return "unsigned char";
    case VTK_SHORT:
      return "short";
    case VTK_UNSIGNED_SHORT:
      return "unsigned short";
    case VTK_INT:
      return "int";
    case VTK_UNSIGNED_INT:
      return "unsigned int";
    case VTK_LONG:
      return "long";
    case VTK_UNSIGNED_LONG:
      return "unsigned long";
    case VTK_FLOAT:
      return "float";
    case VTK_DOUBLE:
      return "double";
    case VTK_ID_TYPE:
      return "vtkIdType";
    case VTK_LONG_LONG:
      return "long long";
    case VTK_UNSIGNED_LONG_LONG:
      return "unsigned long long";
    default:
      return "unknown";
  }
}

} // namespace

QWidget* VolumePropertiesWidget::createSectionHeader(const QString& title,
                                                     QWidget* parent)
{
  auto* container = new QWidget(parent);
  auto* layout = new QVBoxLayout(container);
  layout->setContentsMargins(0, 6, 0, 0);
  layout->setSpacing(2);

  auto* label = new QLabel(QString("<b>%1</b>").arg(title), container);
  layout->addWidget(label);

  auto* line = new QFrame(container);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  layout->addWidget(line);

  return container;
}

QString VolumePropertiesWidget::formatSize(size_t num, bool labelAsBytes)
{
  char format = 'f';
  int prec = 1;

  QString ret;
  if (num < 1000) {
    ret = QString::number(num) + " ";
  } else if (num < 1000000) {
    ret = QString::number(static_cast<double>(num) / 1e3, format, prec) +
          " K";
  } else if (num < 1000000000ULL) {
    ret = QString::number(static_cast<double>(num) / 1e6, format, prec) +
          " M";
  } else if (num < 1000000000000ULL) {
    ret = QString::number(static_cast<double>(num) / 1e9, format, prec) +
          " G";
  } else {
    ret = QString::number(static_cast<double>(num) / 1e12, format, prec) +
          " T";
  }

  if (labelAsBytes) {
    ret += "B";
  }

  return ret;
}

VolumePropertiesWidget::VolumePropertiesWidget(QWidget* parent)
  : QWidget(parent)
{
  m_scalarsModel = new VolumeScalarsModel(this);

  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(4, 4, 4, 4);
  mainLayout->setSpacing(4);

  // --- Label section ---
  mainLayout->addWidget(createSectionHeader("Label", this));
  m_labelEdit = new QLineEdit(this);
  mainLayout->addWidget(m_labelEdit);

  // --- Active Scalars section ---
  mainLayout->addWidget(createSectionHeader("Active Scalars", this));
  m_activeScalarsCombo = new QComboBox(this);
  mainLayout->addWidget(m_activeScalarsCombo);

  // --- Component Names ---
  m_componentNamesLabel = new QLabel("Component Names:", this);
  m_componentNamesCombo = new QComboBox(this);
  m_componentNamesCombo->setEditable(true);
  mainLayout->addWidget(m_componentNamesLabel);
  mainLayout->addWidget(m_componentNamesCombo);
  m_componentNamesLabel->hide();
  m_componentNamesCombo->hide();

  // --- Dimensions & Range section ---
  mainLayout->addWidget(createSectionHeader("Dimensions & Range", this));
  m_dimensionsLabel = new QLabel(this);
  m_voxelsLabel = new QLabel(this);
  m_memoryLabel = new QLabel(this);
  mainLayout->addWidget(m_dimensionsLabel);
  mainLayout->addWidget(m_voxelsLabel);
  mainLayout->addWidget(m_memoryLabel);

  // --- Scalars Table ---
  m_scalarsTable = new QTableView(this);
  m_scalarsTable->setModel(m_scalarsModel);
  m_scalarsTable->setMinimumHeight(125);
  m_scalarsTable->setMaximumHeight(200);
  mainLayout->addWidget(m_scalarsTable);

  // --- Units and Size section ---
  mainLayout->addWidget(createSectionHeader("Units and Size", this));

  auto* sizeWidget = new QWidget(this);
  auto* sizeLayout = new QGridLayout(sizeWidget);
  sizeLayout->setContentsMargins(0, 0, 0, 0);

  // Physical lengths row
  sizeLayout->addWidget(new QLabel("(", sizeWidget), 0, 0);
  for (int i = 0; i < 3; ++i) {
    m_lengthBoxes[i] = new QLineEdit(sizeWidget);
    m_lengthBoxes[i]->setValidator(new QDoubleValidator(m_lengthBoxes[i]));
    sizeLayout->addWidget(m_lengthBoxes[i], 0, 1 + i * 2);
    if (i < 2) {
      sizeLayout->addWidget(new QLabel("x", sizeWidget), 0, 2 + i * 2);
    }
  }
  sizeLayout->addWidget(new QLabel(")", sizeWidget), 0, 6);
  m_unitBox = new QLineEdit(sizeWidget);
  m_unitBox->setPlaceholderText("units");
  m_unitBox->setMaximumWidth(60);
  sizeLayout->addWidget(m_unitBox, 0, 7);

  // Separator line
  auto* sizeSep = new QFrame(sizeWidget);
  sizeSep->setFrameShape(QFrame::HLine);
  sizeSep->setFrameShadow(QFrame::Sunken);
  sizeLayout->addWidget(sizeSep, 1, 0, 1, 8);

  // Voxel sizes row
  sizeLayout->addWidget(new QLabel("(", sizeWidget), 2, 0);
  for (int i = 0; i < 3; ++i) {
    m_voxelSizeBoxes[i] = new QLineEdit(sizeWidget);
    m_voxelSizeBoxes[i]->setValidator(
      new QDoubleValidator(m_voxelSizeBoxes[i]));
    sizeLayout->addWidget(m_voxelSizeBoxes[i], 2, 1 + i * 2);
    if (i < 2) {
      sizeLayout->addWidget(new QLabel("x", sizeWidget), 2, 2 + i * 2);
    }
  }
  sizeLayout->addWidget(new QLabel(")", sizeWidget), 2, 6);
  sizeLayout->addWidget(new QLabel("voxel sizes", sizeWidget), 2, 7);

  mainLayout->addWidget(sizeWidget);

  // --- Transformations section (Origin, Rotation, Interaction) ---
  mainLayout->addWidget(createSectionHeader("Transformations", this));
  auto* transformWidget = new QWidget(this);
  auto* transformLayout = new QGridLayout(transformWidget);
  transformLayout->setContentsMargins(0, 0, 0, 0);
  transformLayout->setSpacing(2);

  const char* axisLabels[] = { "X", "Y", "Z" };
  for (int i = 0; i < 3; ++i) {
    transformLayout->addWidget(
      new QLabel(axisLabels[i], transformWidget), 0, 1 + i, Qt::AlignHCenter);
  }

  // Origin row
  transformLayout->addWidget(new QLabel("Origin:", transformWidget), 1, 0);
  for (int i = 0; i < 3; ++i) {
    m_originBoxes[i] = new QLineEdit(transformWidget);
    m_originBoxes[i]->setValidator(new QDoubleValidator(m_originBoxes[i]));
    m_originBoxes[i]->setAlignment(Qt::AlignCenter);
    transformLayout->addWidget(m_originBoxes[i], 1, 1 + i);
  }

  // Rotation row
  transformLayout->addWidget(new QLabel("Rotation:", transformWidget), 2, 0);
  for (int i = 0; i < 3; ++i) {
    m_orientationBoxes[i] = new QLineEdit(transformWidget);
    m_orientationBoxes[i]->setValidator(
      new QDoubleValidator(m_orientationBoxes[i]));
    m_orientationBoxes[i]->setAlignment(Qt::AlignCenter);
    transformLayout->addWidget(m_orientationBoxes[i], 2, 1 + i);
  }

  // Interaction group
  m_interactionGroup = new QGroupBox("Interaction", transformWidget);
  auto* interactionLayout = new QGridLayout(m_interactionGroup);
  m_interactTranslate = new QCheckBox("Translate", m_interactionGroup);
  m_interactTranslate->setToolTip(
    "Translate by either left-clicking and dragging the central handle, "
    "or by middle-clicking and dragging the data source.");
  m_interactRotate = new QCheckBox("Rotate", m_interactionGroup);
  m_interactRotate->setToolTip(
    "Rotate by left-clicking a face and dragging it. "
    "Rotation interactions are performed about the center of the data "
    "source.");
  m_interactScale = new QCheckBox("Scale", m_interactionGroup);
  m_interactScale->setToolTip(
    "Rescale individual axes by left-clicking the handles on the faces "
    "and dragging them. Rescale with fixed aspect ratio by right-clicking "
    "the data source and moving the mouse.");
  interactionLayout->addWidget(m_interactTranslate, 0, 0);
  interactionLayout->addWidget(m_interactRotate, 0, 1);
  interactionLayout->addWidget(m_interactScale, 0, 2);
  transformLayout->addWidget(m_interactionGroup, 3, 0, 1, 4);

  mainLayout->addWidget(transformWidget);

  // --- Tilt Angles section (initially hidden) ---
  m_tiltAnglesHeader = createSectionHeader("Tilt Angles", this);
  mainLayout->addWidget(m_tiltAnglesHeader);
  m_tiltAnglesTable = new QTableWidget(this);
  m_tiltAnglesTable->setMinimumHeight(125);
  m_tiltAnglesTable->setMaximumHeight(250);
  m_tiltAnglesTable->installEventFilter(this);
  mainLayout->addWidget(m_tiltAnglesTable);
  m_saveTiltAnglesButton = new QPushButton("Save Tilt Angles...", this);
  mainLayout->addWidget(m_saveTiltAnglesButton);
  m_tiltAnglesHeader->hide();
  m_tiltAnglesTable->hide();
  m_saveTiltAnglesButton->hide();

  // --- Time Series section (initially hidden) ---
  m_timeSeriesHeader = createSectionHeader("Time Series", this);
  mainLayout->addWidget(m_timeSeriesHeader);
  m_timeSeriesGroup = new QWidget(this);
  auto* tsLayout = new QVBoxLayout(m_timeSeriesGroup);
  tsLayout->setContentsMargins(0, 0, 0, 0);
  m_showTimeSeriesLabel = new QCheckBox("Show Time Series Label", this);
  tsLayout->addWidget(m_showTimeSeriesLabel);
  m_editTimeSeriesButton = new QPushButton("Edit Time Series...", this);
  tsLayout->addWidget(m_editTimeSeriesButton);
  mainLayout->addWidget(m_timeSeriesGroup);
  m_timeSeriesHeader->hide();
  m_timeSeriesGroup->hide();

  // --- Vertical spacer ---
  mainLayout->addStretch();

  // --- Connect signals ---
  connect(m_labelEdit, &QLineEdit::editingFinished, this,
          &VolumePropertiesWidget::onLabelEdited);
  connect(m_activeScalarsCombo, &QComboBox::currentTextChanged, this,
          &VolumePropertiesWidget::onActiveScalarsChanged);
  connect(m_scalarsModel, &VolumeScalarsModel::activeScalarsChanged, this,
          &VolumePropertiesWidget::onActiveScalarsChanged);
  connect(m_scalarsModel, &VolumeScalarsModel::scalarsRenamed, this,
          &VolumePropertiesWidget::onScalarsRenamed);
  connect(m_componentNamesCombo, &QComboBox::currentTextChanged, this,
          &VolumePropertiesWidget::onComponentNameEdited);
  connect(m_unitBox, &QLineEdit::editingFinished, this,
          &VolumePropertiesWidget::onUnitsEdited);

  for (int i = 0; i < 3; ++i) {
    connect(m_lengthBoxes[i], &QLineEdit::editingFinished, this,
            [this, i]() { onLengthEdited(i); });
    connect(m_voxelSizeBoxes[i], &QLineEdit::editingFinished, this,
            [this, i]() { onVoxelSizeEdited(i); });
    connect(m_originBoxes[i], &QLineEdit::editingFinished, this,
            [this, i]() { onOriginEdited(i); });
    connect(m_orientationBoxes[i], &QLineEdit::editingFinished, this,
            [this, i]() { onOrientationEdited(i); });
  }

  // Tilt angles signals
  connect(m_saveTiltAnglesButton, &QPushButton::clicked, this,
          &VolumePropertiesWidget::saveTiltAngles);

  // Time series signals
  connect(m_editTimeSeriesButton, &QPushButton::clicked, this,
          &VolumePropertiesWidget::editTimeSeries);

  clear();
}

VolumePropertiesWidget::~VolumePropertiesWidget()
{
  auto& itw = InteractiveTransformWidget::instance();
  if (itw.currentUser() == this) {
    itw.release(this);
  }
}

void VolumePropertiesWidget::setOutputPort(OutputPort* port)
{
  if (m_port) {
    disconnect(m_port, nullptr, this, nullptr);
  }

  m_port = port;
  m_scalarIndexes.clear();

  if (m_port) {
    connect(m_port, &OutputPort::dataChanged, this,
            &VolumePropertiesWidget::updateData);
    connect(m_port, &OutputPort::metadataChanged, this,
            &VolumePropertiesWidget::updateTransformFields);
    updateData();
  } else {
    clear();
  }
}

OutputPort* VolumePropertiesWidget::outputPort() const
{
  return m_port;
}

VolumeData* VolumePropertiesWidget::volumeData() const
{
  if (!m_port || !m_port->hasData()) {
    return nullptr;
  }

  try {
    auto ptr = m_port->data().value<VolumeDataPtr>();
    return ptr.get();
  } catch (const std::bad_any_cast&) {
    return nullptr;
  }
}

void VolumePropertiesWidget::updateData()
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    clear();
    return;
  }

  // Block signals to prevent feedback loops
  QSignalBlocker labelBlocker(m_labelEdit);
  QSignalBlocker scalarsBlocker(m_activeScalarsCombo);
  QSignalBlocker unitBlocker(m_unitBox);

  // Label
  m_labelEdit->setText(vol->label());

  // Dimensions
  auto dims = vol->dimensions();
  m_dimensionsLabel->setText(
    QString("Dimensions: %1 x %2 x %3").arg(dims[0]).arg(dims[1]).arg(dims[2]));

  // Voxels count
  qint64 numVoxels =
    static_cast<qint64>(dims[0]) * dims[1] * dims[2];
  m_voxelsLabel->setText("Voxels: " + formatSize(numVoxels));

  // Memory size (vtkImageData::GetActualMemorySize returns kilobytes)
  auto* imageData = vol->imageData();
  size_t memSize =
    static_cast<size_t>(imageData->GetActualMemorySize()) * 1000;
  m_memoryLabel->setText("Memory: " + formatSize(memSize, true));

  // Units
  m_unitBox->setText(vol->units());

  // Physical lengths and voxel sizes
  auto spacing = vol->spacing();
  auto extent = vol->extent();
  for (int i = 0; i < 3; ++i) {
    int dimSize = extent[2 * i + 1] - extent[2 * i] + 1;
    double length = spacing[i] * dimSize;
    {
      QSignalBlocker b(m_lengthBoxes[i]);
      m_lengthBoxes[i]->setText(QString::number(length));
    }
    {
      QSignalBlocker b(m_voxelSizeBoxes[i]);
      m_voxelSizeBoxes[i]->setText(QString::number(spacing[i]));
    }
  }

  // Origin (display position)
  auto displayPos = vol->displayPosition();
  for (int i = 0; i < 3; ++i) {
    QSignalBlocker b(m_originBoxes[i]);
    m_originBoxes[i]->setText(QString::number(displayPos[i]));
  }

  // Orientation
  auto orient = vol->displayOrientation();
  for (int i = 0; i < 3; ++i) {
    QSignalBlocker b(m_orientationBoxes[i]);
    m_orientationBoxes[i]->setText(QString::number(orient[i]));
  }

  // Scalars arrays info and active scalars combo
  gatherAndUpdateArraysInfo();

  // Component names
  auto* activeScalars = imageData->GetPointData()->GetScalars();
  bool multiComponent =
    activeScalars && activeScalars->GetNumberOfComponents() > 1;
  m_componentNamesLabel->setVisible(multiComponent);
  m_componentNamesCombo->setVisible(multiComponent);
  if (multiComponent) {
    QSignalBlocker cb(m_componentNamesCombo);
    m_componentNamesCombo->clear();
    for (int i = 0; i < activeScalars->GetNumberOfComponents(); ++i) {
      const char* compName = activeScalars->GetComponentName(i);
      m_componentNamesCombo->addItem(
        compName ? QString(compName) : QString("Component %1").arg(i));
    }
  }

  // Tilt angles and time series
  updateTiltAnglesSection();
  updateTimeSeriesSection();

}

void VolumePropertiesWidget::gatherAndUpdateArraysInfo()
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  auto* pointData = vol->imageData()->GetPointData();
  int numArrays = pointData->GetNumberOfArrays();

  // Build index list for consistent ordering (sorted by name on first call)
  if (m_scalarIndexes.isEmpty()) {
    QList<QPair<QString, int>> nameIndex;
    for (int i = 0; i < numArrays; ++i) {
      auto* arr = pointData->GetArray(i);
      if (arr) {
        nameIndex.append({ QString(arr->GetName()), i });
      }
    }
    std::sort(nameIndex.begin(), nameIndex.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (const auto& ni : nameIndex) {
      m_scalarIndexes.append(ni.second);
    }
  }

  // Remove invalid indices
  QList<int> valid;
  for (auto idx : m_scalarIndexes) {
    if (idx < numArrays && pointData->GetArray(idx)) {
      valid.append(idx);
    }
  }
  m_scalarIndexes = valid;

  // Determine current active scalars name
  auto* activeArr = pointData->GetScalars();
  QString activeName = activeArr ? QString(activeArr->GetName()) : QString();

  QList<ScalarArrayInfo> arraysInfo;
  for (auto idx : m_scalarIndexes) {
    auto* array = pointData->GetArray(idx);
    if (!array) {
      continue;
    }

    QString arrayName = QString(array->GetName());
    QString type = dataTypeName(array->GetDataType());
    int numComponents = array->GetNumberOfComponents();
    bool active = (arrayName == activeName);

    QString rangeStr;
    double range[2];
    for (int j = 0; j < numComponents; ++j) {
      if (j != 0) {
        rangeStr.append(", ");
      }
      array->GetRange(range, j);
      rangeStr.append(
        QString("[%1, %2]").arg(range[0]).arg(range[1]));
    }

    arraysInfo.append(
      ScalarArrayInfo(arrayName,
                      type == "string" ? tr("NA") : rangeStr,
                      type, active));
  }

  m_scalarsModel->setArraysInfo(arraysInfo);
  m_scalarsTable->resizeColumnsToContents();
  m_scalarsTable->horizontalHeader()->setSectionResizeMode(
    1, QHeaderView::Stretch);

  // Update active scalars combo
  {
    QSignalBlocker b(m_activeScalarsCombo);
    m_activeScalarsCombo->clear();
    for (const auto& info : arraysInfo) {
      m_activeScalarsCombo->addItem(info.name);
      if (info.active) {
        m_activeScalarsCombo->setCurrentText(info.name);
      }
    }
  }
}

void VolumePropertiesWidget::clear()
{
  m_labelEdit->clear();
  m_activeScalarsCombo->clear();
  m_dimensionsLabel->clear();
  m_voxelsLabel->clear();
  m_memoryLabel->clear();
  m_scalarsModel->setArraysInfo({});
  m_componentNamesLabel->hide();
  m_componentNamesCombo->hide();
  m_unitBox->clear();
  for (int i = 0; i < 3; ++i) {
    m_lengthBoxes[i]->clear();
    m_voxelSizeBoxes[i]->clear();
    m_originBoxes[i]->clear();
    m_orientationBoxes[i]->clear();
  }
  m_interactTranslate->setChecked(false);
  m_interactRotate->setChecked(false);
  m_interactScale->setChecked(false);
  m_tiltAnglesHeader->hide();
  m_tiltAnglesTable->clear();
  m_tiltAnglesTable->setRowCount(0);
  m_tiltAnglesTable->hide();
  m_saveTiltAnglesButton->hide();
  m_timeSeriesHeader->hide();
  m_timeSeriesGroup->hide();
}

void VolumePropertiesWidget::onLabelEdited()
{
  auto* vol = volumeData();
  if (!vol) {
    return;
  }
  vol->setLabel(m_labelEdit->text());
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::onActiveScalarsChanged(const QString& name)
{
  if (name.isEmpty()) {
    return;
  }
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }
  auto* pointData = vol->imageData()->GetPointData();
  auto* current = pointData->GetScalars();
  if (current && QString(current->GetName()) == name) {
    return;
  }
  pointData->SetActiveScalars(name.toUtf8().constData());

  // Bump the imageData MTime so the histogram cache is invalidated
  // (SetActiveScalars only modifies vtkPointData's MTime).
  vol->imageData()->Modified();

  // Rescale color/opacity maps to the new scalar range.
  vol->rescaleColorMap();

  updateData();
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::onComponentNameEdited(const QString& name)
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }
  auto* scalars = vol->imageData()->GetPointData()->GetScalars();
  if (!scalars) {
    return;
  }
  int index = m_componentNamesCombo->currentIndex();
  if (index >= 0 && index < scalars->GetNumberOfComponents()) {
    scalars->SetComponentName(index, name.toUtf8().constData());
    if (m_port) {
      emit m_port->metadataChanged();
    }
  }
}

void VolumePropertiesWidget::onUnitsEdited()
{
  auto* vol = volumeData();
  if (!vol) {
    return;
  }
  vol->setUnits(m_unitBox->text());
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::onLengthEdited(int axis)
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  bool ok = false;
  double newLength = m_lengthBoxes[axis]->text().toDouble(&ok);
  if (!ok) {
    return;
  }

  auto ext = vol->extent();
  int dimSize = ext[2 * axis + 1] - ext[2 * axis] + 1;
  if (dimSize <= 0) {
    return;
  }

  auto sp = vol->spacing();
  sp[axis] = newLength / dimSize;
  vol->setSpacing(sp[0], sp[1], sp[2]);
  updateData();
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::onVoxelSizeEdited(int axis)
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  bool ok = false;
  double newSize = m_voxelSizeBoxes[axis]->text().toDouble(&ok);
  if (!ok) {
    return;
  }

  auto sp = vol->spacing();
  sp[axis] = newSize;
  vol->setSpacing(sp[0], sp[1], sp[2]);
  updateData();
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::onOriginEdited(int axis)
{
  auto* vol = volumeData();
  if (!vol) {
    return;
  }

  bool ok = false;
  double newValue = m_originBoxes[axis]->text().toDouble(&ok);
  if (!ok) {
    return;
  }

  auto pos = vol->displayPosition();
  pos[axis] = newValue;
  vol->setDisplayPosition(pos[0], pos[1], pos[2]);
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::onOrientationEdited(int axis)
{
  auto* vol = volumeData();
  if (!vol) {
    return;
  }

  bool ok = false;
  double newValue = m_orientationBoxes[axis]->text().toDouble(&ok);
  if (!ok) {
    return;
  }

  auto orient = vol->displayOrientation();
  orient[axis] = newValue;
  vol->setDisplayOrientation(orient[0], orient[1], orient[2]);
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::updateTransformFields()
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  auto spacing = vol->spacing();
  auto displayPos = vol->displayPosition();
  auto orient = vol->displayOrientation();

  int dims[3];
  vol->imageData()->GetDimensions(dims);

  for (int i = 0; i < 3; ++i) {
    {
      QSignalBlocker b(m_voxelSizeBoxes[i]);
      m_voxelSizeBoxes[i]->setText(QString::number(spacing[i]));
    }
    {
      QSignalBlocker b(m_lengthBoxes[i]);
      double length = spacing[i] * (dims[i] - 1);
      m_lengthBoxes[i]->setText(QString::number(length));
    }
    {
      QSignalBlocker b(m_originBoxes[i]);
      m_originBoxes[i]->setText(QString::number(displayPos[i]));
    }
    {
      QSignalBlocker b(m_orientationBoxes[i]);
      m_orientationBoxes[i]->setText(QString::number(orient[i]));
    }
  }

  // Keep the interactive box widget in sync if it's active and the change
  // came from the text fields (not from the widget itself).
  if (!m_interacting) {
    auto& itw = InteractiveTransformWidget::instance();
    if (itw.currentUser() == this) {
      placeTransformWidget();
    }
  }
}

void VolumePropertiesWidget::onScalarsRenamed(const QString& oldName,
                                              const QString& newName)
{
  auto* vol = volumeData();
  if (!vol || !vol->isValid()) {
    return;
  }

  vol->renameScalarArray(oldName, newName);

  updateData();
  if (m_port) {
    emit m_port->metadataChanged();
  }
}

void VolumePropertiesWidget::updateTiltAnglesSection()
{
  auto* vol = volumeData();
  if (!vol || !vol->hasTiltAngles()) {
    m_tiltAnglesHeader->hide();
    m_tiltAnglesTable->hide();
    m_saveTiltAnglesButton->hide();
    return;
  }

  disconnect(m_tiltAnglesTable, &QTableWidget::cellChanged, this,
             &VolumePropertiesWidget::onTiltAnglesModified);

  m_tiltAnglesHeader->show();
  m_tiltAnglesTable->show();
  m_saveTiltAnglesButton->show();

  QVector<double> tiltAngles = vol->tiltAngles();
  QVector<int> scanIds = vol->scanIds();
  bool hasScanIds = scanIds.size() == tiltAngles.size() && !scanIds.isEmpty();

  m_tiltAnglesTable->setRowCount(tiltAngles.size());
  int numCols = hasScanIds ? 2 : 1;
  m_tiltAnglesTable->setColumnCount(numCols);
  int tiltCol = hasScanIds ? 1 : 0;
  for (int i = 0; i < tiltAngles.size(); ++i) {
    if (hasScanIds) {
      auto* scanItem = new QTableWidgetItem();
      scanItem->setData(Qt::DisplayRole, QString::number(scanIds[i]));
      scanItem->setFlags(scanItem->flags() & ~Qt::ItemIsEditable);
      m_tiltAnglesTable->setItem(i, 0, scanItem);
    }
    auto* item = new QTableWidgetItem();
    item->setData(Qt::DisplayRole, QString::number(tiltAngles[i]));
    m_tiltAnglesTable->setItem(i, tiltCol, item);
  }
  QStringList headers;
  if (hasScanIds) {
    headers << "Scan ID";
  }
  headers << "Tilt Angle";
  m_tiltAnglesTable->setHorizontalHeaderLabels(headers);
  m_tiltAnglesTable->horizontalHeader()->setStretchLastSection(true);

  connect(m_tiltAnglesTable, &QTableWidget::cellChanged, this,
          &VolumePropertiesWidget::onTiltAnglesModified);
}

void VolumePropertiesWidget::onTiltAnglesModified(int row, int column)
{
  auto* vol = volumeData();
  if (!vol) {
    return;
  }
  // Tilt angles are in the last column (column 1 when scan IDs present,
  // column 0 otherwise).
  int tiltCol = (vol->hasScanIds() && vol->scanIds().size() ==
                 vol->tiltAngles().size()) ? 1 : 0;
  if (column != tiltCol) {
    return;
  }
  auto* item = m_tiltAnglesTable->item(row, column);
  bool ok = false;
  double value = item->data(Qt::DisplayRole).toDouble(&ok);
  if (ok) {
    auto angles = vol->tiltAngles();
    if (row < angles.size()) {
      angles[row] = value;
      vol->setTiltAngles(angles);
      emit volumeDataModified();
    }
  }
}

void VolumePropertiesWidget::saveTiltAngles()
{
  auto* vol = volumeData();
  if (!vol || !vol->hasTiltAngles()) {
    return;
  }

  QString fileName = QFileDialog::getSaveFileName(
    this, "Save Tilt Angles", QString(),
    "TXT Files (*.txt);;All Files (*)");
  if (fileName.isEmpty()) {
    return;
  }
  if (!fileName.endsWith(".txt", Qt::CaseInsensitive)) {
    fileName += ".txt";
  }

  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Error",
      "Could not open file for writing: " + file.errorString());
    return;
  }

  // One line per image: the scan ID first, when there are scan IDs, then
  // the tilt angle
  QVector<double> tiltAngles = vol->tiltAngles();
  QVector<int> scanIds = vol->scanIds();
  bool hasScanIds = scanIds.size() == tiltAngles.size() && !scanIds.isEmpty();
  QTextStream out(&file);
  for (int i = 0; i < tiltAngles.size(); ++i) {
    if (hasScanIds) {
      out << scanIds[i] << " ";
    }
    out << tiltAngles[i] << "\n";
  }
}

bool VolumePropertiesWidget::eventFilter(QObject* obj, QEvent* event)
{
  auto* ke = dynamic_cast<QKeyEvent*>(event);
  if (ke && obj == m_tiltAnglesTable) {
    if (ke->matches(QKeySequence::Paste) && ke->type() == QEvent::KeyPress) {
      auto* clipboard = QGuiApplication::clipboard();
      const auto* mimeData = clipboard->mimeData();
      if (mimeData->hasText()) {
        QStringList rows = mimeData->text().split("\n");
        QStringList angleStrings;
        for (const QString& row : rows) {
          QString trimmed = row.trimmed();
          if (!trimmed.isEmpty()) {
            angleStrings << trimmed.split("\t")[0];
          }
        }
        // Validate all values are numbers
        for (const QString& s : angleStrings) {
          bool ok;
          s.toDouble(&ok);
          if (!ok) {
            QMessageBox::warning(this, "Error",
              QString("Pasted tilt angle \"%1\" is not a number").arg(s));
            return true;
          }
        }
        auto ranges = m_tiltAnglesTable->selectedRanges();
        if (ranges.size() != 1) {
          QMessageBox::warning(this, "Error",
            "Pasting is not supported with non-continuous selections");
          return true;
        }
        if (ranges[0].rowCount() > 1 &&
            ranges[0].rowCount() != angleStrings.size()) {
          QMessageBox::warning(this, "Error",
            QString("Cells selected (%1) does not match angles to paste (%2)")
              .arg(ranges[0].rowCount())
              .arg(angleStrings.size()));
          return true;
        }

        auto* vol = volumeData();
        if (!vol) {
          return true;
        }
        auto angles = vol->tiltAngles();
        int startRow = ranges[0].topRow();
        for (int i = 0;
             i < angleStrings.size() && i + startRow < angles.size(); ++i) {
          angles[i + startRow] = angleStrings[i].toDouble();
        }
        vol->setTiltAngles(angles);
        updateTiltAnglesSection();
        emit volumeDataModified();
      }
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void VolumePropertiesWidget::updateTimeSeriesSection()
{
  auto* vol = volumeData();
  bool visible = vol && vol->hasTimeSteps();

  m_timeSeriesHeader->setVisible(visible);
  m_timeSeriesGroup->setVisible(visible);
}

void VolumePropertiesWidget::editTimeSeries()
{
  auto* vol = volumeData();
  if (!vol || !vol->hasTimeSteps()) {
    return;
  }

  auto steps = vol->timeSteps();
  QStringList labels;
  for (const auto& step : steps) {
    labels.append(step.label);
  }

  // Simple edit dialog for time series labels
  auto* dlg = new QDialog(this);
  dlg->setWindowTitle("Edit Time Series Labels");
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  auto* layout = new QVBoxLayout(dlg);

  auto* table = new QTableWidget(labels.size(), 1, dlg);
  table->setHorizontalHeaderLabels({ "Label" });
  table->horizontalHeader()->setStretchLastSection(true);
  for (int i = 0; i < labels.size(); ++i) {
    table->setItem(i, 0, new QTableWidgetItem(labels[i]));
  }
  layout->addWidget(table);

  auto* buttons = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

  connect(dlg, &QDialog::accepted, this, [this, vol, table, steps]() {
    auto newSteps = steps;
    for (int i = 0; i < newSteps.size() && i < table->rowCount(); ++i) {
      auto* item = table->item(i, 0);
      if (item) {
        newSteps[i].label = item->text();
      }
    }
    vol->setTimeSteps(newSteps);
    emit volumeDataModified();
  });

  dlg->resize(300, 400);
  dlg->show();
}

// --- Interactive transform widget integration ---

void VolumePropertiesWidget::setupInteractiveTransform()
{
  auto& itw = InteractiveTransformWidget::instance();

  connect(m_interactTranslate, &QCheckBox::clicked, this,
          &VolumePropertiesWidget::updateTransformWidget);
  connect(m_interactRotate, &QCheckBox::clicked, this,
          &VolumePropertiesWidget::updateTransformWidget);
  connect(m_interactScale, &QCheckBox::clicked, this,
          &VolumePropertiesWidget::updateTransformWidget);

  connect(&itw, &InteractiveTransformWidget::widgetReleased, this,
          [this, &itw]() {
            if (itw.currentUser() != this) {
              m_interactTranslate->setChecked(false);
              m_interactRotate->setChecked(false);
              m_interactScale->setChecked(false);
            }
          });

  connect(&itw, &InteractiveTransformWidget::transformChanged, this,
          &VolumePropertiesWidget::onTransformChanged);
}

void VolumePropertiesWidget::placeTransformWidget()
{
  if (!m_port || !m_port->hasData()) {
    return;
  }
  try {
    auto ptr = m_port->data().value<VolumeDataPtr>();
    if (ptr && ptr->isValid()) {
      auto& itw = InteractiveTransformWidget::instance();
      auto ext = ptr->extent();
      double bounds[6];
      for (int i = 0; i < 6; ++i) {
        bounds[i] = static_cast<double>(ext[i]);
      }
      itw.setBounds(bounds);

      auto pos = ptr->displayPosition();
      auto orient = ptr->displayOrientation();
      auto spacing = ptr->spacing();
      itw.setTransform(pos.data(), orient.data(), spacing.data());
    }
  } catch (const std::bad_any_cast&) {
  }
}

void VolumePropertiesWidget::updateTransformWidget()
{
  auto& itw = InteractiveTransformWidget::instance();
  bool translate = m_interactTranslate->isChecked();
  bool rotate = m_interactRotate->isChecked();
  bool scale = m_interactScale->isChecked();
  bool anyEnabled = translate || rotate || scale;

  if (!anyEnabled) {
    if (itw.currentUser() == this) {
      itw.release(this);
    }
    return;
  }

  bool freshAcquire = (itw.currentUser() != this);
  if (freshAcquire && !itw.acquire(this)) {
    m_interactTranslate->setChecked(false);
    m_interactRotate->setChecked(false);
    m_interactScale->setChecked(false);
    return;
  }

  if (freshAcquire) {
    itw.setView(ActiveObjects::instance().activePqView());
    placeTransformWidget();
  }

  itw.setTranslationEnabled(translate);
  itw.setRotationEnabled(rotate);
  itw.setScalingEnabled(scale);
}

void VolumePropertiesWidget::onTransformChanged(const double position[3],
                                                const double orientation[3],
                                                const double scale[3])
{
  auto& itw = InteractiveTransformWidget::instance();
  if (itw.currentUser() != this || !m_port || !m_port->hasData()) {
    return;
  }
  try {
    auto ptr = m_port->data().value<VolumeDataPtr>();
    if (ptr && ptr->isValid()) {
      m_interacting = true;
      ptr->setDisplayPosition(position[0], position[1], position[2]);
      ptr->setDisplayOrientation(orientation[0], orientation[1],
                                  orientation[2]);
      ptr->setSpacing(scale[0], scale[1], scale[2]);
      emit m_port->metadataChanged();
      m_interacting = false;
    }
  } catch (const std::bad_any_cast&) {
  }
}

} // namespace pipeline
} // namespace tomviz

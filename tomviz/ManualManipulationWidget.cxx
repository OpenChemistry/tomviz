/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ManualManipulationWidget.h"
#include "ui_ManualManipulationWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleManager.h"
#include "OperatorPython.h"
#include "Utilities.h"

#include <pqApplicationCore.h>
#include <pqProxy.h>
#include <pqSettings.h>

#include <vtkImageData.h>
#include <vtkSMParaViewPipelineControllerWithRendering.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMSessionProxyManager.h>
#include <vtkSMSourceProxy.h>
#include <vtkSMViewProxy.h>
#include <vtkTransform.h>

namespace tomviz {

class ManualManipulationWidget::Internal : public QObject
{
public:
  Ui::ManualManipulationWidget ui;
  QPointer<Operator> op;
  vtkSmartPointer<vtkImageData> image;
  QPointer<ManualManipulationWidget> parent;
  QPointer<DataSource> dataSource;
  QPointer<DataSource> referenceData;
  double savedReferencePosition[3] = { 0, 0, 0 };
  vtkSmartPointer<vtkSMSourceProxy> originalOutlineSource;
  vtkSmartPointer<vtkSMProxy> originalOutlineRepresentation;
  vtkNew<vtkSMParaViewPipelineControllerWithRendering> pipelineController;
  double cachedBounds[6] = { 0, 0, 0, 0, 0, 0 };

  Internal(Operator* o, vtkSmartPointer<vtkImageData> img,
           ManualManipulationWidget* p)
    : op(o), image(img), parent(p)
  {
    // Must call setupUi() before using p in any way
    ui.setupUi(p);
    setParent(p);

    if (op->childDataSource() && !op->isEditing()) {
      dataSource = op->childDataSource();
    } else if (op->dataSource()) {
      dataSource = op->dataSource();
    } else {
      dataSource = ActiveObjects::instance().activeDataSource();
    }

    if (op->isEditing() && op->childDataSource()) {
      // Mark the units of the child data source as modified so that when
      // this widget modifies the spacing of the parent data source, it won't
      // propagate down to the child.
      auto* cds = op->childDataSource();
      cds->setSpacing(cds->getSpacing(), true);
    }

    // Make sure this is the active data source
    ActiveObjects::instance().setActiveDataSource(dataSource);
    fixInteractionDataSource();

    populateReferenceDataCheckBoxes();
    createOriginalOutline();
    setupConnections();
    enableAllInteraction();
    updateGui();
  }

  ~Internal()
  {
    restoreReferenceDataPosition();
    unfixInteractionDataSource();
    removeOriginalOutline();
    disableAllInteraction();
  }

  void setupConnections()
  {
    connect(dataSource, &DataSource::dataPropertiesChanged, this,
            &Internal::onDataSourcePropertiesChanged);

    ActiveObjects& activeObjects = ActiveObjects::instance();

    connect(&activeObjects, &ActiveObjects::translationStateChanged, this,
            &Internal::updateInteractionCheckboxes);
    connect(&activeObjects, &ActiveObjects::rotationStateChanged, this,
            &Internal::updateInteractionCheckboxes);
    connect(&activeObjects, &ActiveObjects::scalingStateChanged, this,
            &Internal::updateInteractionCheckboxes);

    connect(ui.interactTranslate, &QCheckBox::clicked, &activeObjects,
            &ActiveObjects::enableTranslation);
    connect(ui.interactRotate, &QCheckBox::clicked, &activeObjects,
            &ActiveObjects::enableRotation);
    connect(ui.interactScale, &QCheckBox::clicked, &activeObjects,
            &ActiveObjects::enableScaling);

    connect(dataSource, &DataSource::displayPositionChanged, this,
            &Internal::updateGui);
    connect(dataSource, &DataSource::displayOrientationChanged, this,
            &Internal::updateGui);

    QList<QDoubleSpinBox*> shiftWidgets = { ui.shiftX, ui.shiftY, ui.shiftZ };
    QList<QDoubleSpinBox*> rotateWidgets = { ui.rotateX, ui.rotateY,
                                             ui.rotateZ };
    QList<QDoubleSpinBox*> scaleWidgets = { ui.scaleX, ui.scaleY, ui.scaleZ };

    for (int axis = 0; axis < 3; ++axis) {
      connect(shiftWidgets[axis], &QDoubleSpinBox::editingFinished, this,
              [this, axis, shiftWidgets]() {
                this->setShiftValue(axis, shiftWidgets[axis]->value());
              });
      connect(rotateWidgets[axis], &QDoubleSpinBox::editingFinished, this,
              [this, axis, rotateWidgets]() {
                this->setRotationValue(axis, rotateWidgets[axis]->value());
              });
      connect(scaleWidgets[axis], &QDoubleSpinBox::editingFinished, this,
              [this, axis, scaleWidgets]() {
                this->setScalingValue(axis, scaleWidgets[axis]->value());
              });
    }

    connect(ui.selectedReferenceData,
            QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &Internal::onSelectedReferenceDataChanged);
    connect(ui.alignVoxelsWithReference, &QCheckBox::toggled, this,
            &Internal::onAlignVoxelsWithReferenceToggled);
  }

  QList<QVariant> scaling()
  {
    auto* scaling = dataSource->getSpacing();
    return { scaling[0], scaling[1], scaling[2] };
  }

  QList<QVariant> rotation()
  {
    const auto* orientation = dataSource->displayOrientation();
    return { orientation[0], orientation[1], orientation[2] };
  }

  QList<QVariant> shift()
  {
    // Compute the center
    // Since we rotate about the center, this is the one point that won't
    // move due to rotation. We'll transform its coordinates and then
    // compute the difference to figure out what our shift should be.
    double center[3];
    computeCenter(center);

    vtkNew<vtkTransform> t;

    // Translate
    t->Translate(dataSource->displayPosition());

    // Rotate
    const auto* orientation = dataSource->displayOrientation();
    t->RotateZ(orientation[2]);
    t->RotateX(orientation[0]);
    t->RotateY(orientation[1]);

    // Transform it to the new coordinates
    double physicalShift[3];
    t->TransformPoint(center, physicalShift);

    // Now get the difference
    for (auto i = 0; i < 3; ++i) {
      physicalShift[i] -= center[i];
    }

    return { physicalShift[0], physicalShift[1], physicalShift[2] };
  }

  QList<QVariant> voxelShift()
  {
    // Get the shift as voxel shift values
    double physicalShift[3];
    auto physicalShiftList = shift();
    for (int i = 0; i < 3; ++i) {
      physicalShift[i] = physicalShiftList[i].toDouble();
    }

    double lengths[3];
    dataSource->getPhysicalDimensions(lengths);
    const auto* dims = dataSource->imageData()->GetDimensions();
    int shifts[3];
    for (auto i = 0; i < 3; ++i) {
      shifts[i] = std::round(physicalShift[i] / lengths[i] * dims[i]);
    }

    return { shifts[0], shifts[1], shifts[2] };
  }

  void setScaling(double* scale) { dataSource->setSpacing(scale); }

  void setRotation(double* rotation)
  {
    double physicalShift[3];
    auto shiftList = shift();
    for (int i = 0; i < 3; ++i) {
      physicalShift[i] = shiftList[i].toDouble();
    }
    dataSource->setDisplayOrientation(rotation);
    // Change the data positions so that the shift stays fixed
    setShift(physicalShift);
  }

  void setShift(double* physicalShift)
  {
    double center[3];
    computeCenter(center);

    // Determine where the data will be after its current rotations
    vtkNew<vtkTransform> t;

    // Rotate
    const auto* orientation = dataSource->displayOrientation();
    t->RotateZ(orientation[2]);
    t->RotateX(orientation[0]);
    t->RotateY(orientation[1]);

    double rotatedCenter[3];
    t->TransformPoint(center, rotatedCenter);

    // Set the display position to the difference
    double newPosition[3];
    for (int i = 0; i < 3; ++i) {
      newPosition[i] = physicalShift[i] - rotatedCenter[i] + center[i];
    }

    dataSource->setDisplayPosition(newPosition);
  }

  void setVoxelShift(int* shift)
  {
    double physicalShift[3];
    double lengths[3];
    dataSource->getPhysicalDimensions(lengths);
    const auto* dims = dataSource->imageData()->GetDimensions();
    for (auto i = 0; i < 3; ++i) {
      physicalShift[i] = shift[i] * lengths[i] / dims[i];
    }

    setShift(physicalShift);
  }

  bool alignWithReference() { return ui.alignVoxelsWithReference->isChecked(); }

  void setAlignWithReference(bool b)
  {
    ui.alignVoxelsWithReference->setChecked(b);
  }

  QList<QVariant> referenceSpacing()
  {
    QList<QVariant> ret;
    QList<QDoubleSpinBox*> widgets = { ui.referenceSpacingX,
                                       ui.referenceSpacingY,
                                       ui.referenceSpacingZ };
    for (auto* w : widgets) {
      ret.append(w->value());
    }
    return ret;
  }

  void setReferenceSpacing(const double* spacing)
  {
    QList<QDoubleSpinBox*> widgets = { ui.referenceSpacingX,
                                       ui.referenceSpacingY,
                                       ui.referenceSpacingZ };
    for (int i = 0; i < 3; ++i) {
      widgets[i]->setValue(spacing[i]);
    }
  }

  QList<QVariant> referenceShape()
  {
    QList<QVariant> ret;
    QList<QSpinBox*> widgets = { ui.referenceShapeX, ui.referenceShapeY,
                                 ui.referenceShapeZ };

    for (auto* w : widgets) {
      ret.append(w->value());
    }

    return ret;
  }

  void setReferenceShape(const int* shape)
  {
    QList<QSpinBox*> widgets = { ui.referenceShapeX, ui.referenceShapeY,
                                 ui.referenceShapeZ };
    for (int i = 0; i < 3; ++i) {
      widgets[i]->setValue(shape[i]);
    }
  }

  void createOriginalOutline()
  {
    auto* vtkView = ActiveObjects::instance().activeView();

    auto pxm = dataSource->proxy()->GetSessionProxyManager();

    // Create the outline filter.
    vtkSmartPointer<vtkSMProxy> proxy;
    proxy.TakeReference(pxm->NewProxy("sources", "OutlineSource"));

    dataSource->getBounds(cachedBounds);

    originalOutlineSource = vtkSMSourceProxy::SafeDownCast(proxy);
    pipelineController->PreInitializeProxy(originalOutlineSource);
    vtkSMPropertyHelper(originalOutlineSource, "Bounds").Set(cachedBounds, 6);
    pipelineController->PostInitializeProxy(originalOutlineSource);
    pipelineController->RegisterPipelineProxy(originalOutlineSource);

    // Create the representation for it.
    originalOutlineRepresentation =
      pipelineController->Show(originalOutlineSource, 0, vtkView);

    // Set the color
    static const double color[3] = { 1, 0, 0 };
    vtkSMPropertyHelper(originalOutlineRepresentation, "DiffuseColor")
      .Set(color, 3);
    vtkSMPropertyHelper(originalOutlineRepresentation, "LineWidth").Set(5);

    originalOutlineRepresentation->UpdateVTKObjects();

    // Give the proxy a friendly name for the GUI/Python world.
    if (auto p = convert<pqProxy*>(proxy)) {
      p->rename("OriginalPositionOutline");
    }

    render();
  }

  void removeOriginalOutline()
  {
    if (originalOutlineRepresentation) {
      pipelineController->UnRegisterProxy(originalOutlineRepresentation);
      originalOutlineRepresentation = nullptr;
    }

    if (originalOutlineSource) {
      pipelineController->UnRegisterProxy(originalOutlineSource);
      originalOutlineSource = nullptr;
    }

    render();
  }

  void render()
  {
    auto* view = ActiveObjects::instance().activeView();
    if (!view) {
      return;
    }
    view->StillRender();
  }

  void onDataSourcePropertiesChanged()
  {
    const double tol = 1.e-8;

    double bounds[6];
    dataSource->getBounds(bounds);
    bool boundsChanged = false;
    for (int i = 0; i < 6; ++i) {
      if (std::abs(bounds[i] - cachedBounds[i]) > tol) {
        boundsChanged = true;
        break;
      }
    }

    if (boundsChanged) {
      onBoundsChanged();
    }
    updateGui();
  }

  void onBoundsChanged()
  {
    double bounds[6];
    dataSource->getBounds(bounds);

    for (int i = 0; i < 6; ++i) {
      cachedBounds[i] = bounds[i];
    }

    vtkSMPropertyHelper(originalOutlineSource, "Bounds").Set(bounds, 6);
    originalOutlineSource->UpdateVTKObjects();

    alignReferenceDataPosition();
    render();
  }

  void updateGui()
  {
    auto shifts = shift();
    auto rotations = rotation();
    auto scales = scaling();

    QList<QDoubleSpinBox*> shiftWidgets = { ui.shiftX, ui.shiftY, ui.shiftZ };
    QList<QDoubleSpinBox*> rotateWidgets = { ui.rotateX, ui.rotateY,
                                             ui.rotateZ };
    QList<QDoubleSpinBox*> scaleWidgets = { ui.scaleX, ui.scaleY, ui.scaleZ };

    for (int i = 0; i < 3; ++i) {
      shiftWidgets[i]->setValue(shifts[i].toDouble());
      rotateWidgets[i]->setValue(rotations[i].toDouble());
      scaleWidgets[i]->setValue(scales[i].toDouble());
    }
  }

  void setScalingValue(int axis, double value)
  {
    double scales[3];
    dataSource->getSpacing(scales);

    scales[axis] = value;
    setScaling(scales);
  }

  void setShiftValue(int axis, double value)
  {
    double shifts[3];
    auto previousShifts = shift();
    for (int i = 0; i < 3; ++i) {
      shifts[i] = previousShifts[i].toDouble();
    }

    shifts[axis] = value;
    setShift(shifts);
  }

  void setRotationValue(int axis, double value)
  {
    double orientation[3];
    for (int i = 0; i < 3; ++i) {
      orientation[i] = dataSource->displayOrientation()[i];
    }
    orientation[axis] = value;
    setRotation(orientation);
  }

  void enableAllInteraction()
  {
    ActiveObjects& activeObjects = ActiveObjects::instance();

    activeObjects.enableTranslation(true);
    activeObjects.enableRotation(true);
    activeObjects.enableScaling(true);
  }

  void disableAllInteraction()
  {
    ActiveObjects& activeObjects = ActiveObjects::instance();

    activeObjects.enableTranslation(false);
    activeObjects.enableRotation(false);
    activeObjects.enableScaling(false);
  }

  void fixInteractionDataSource()
  {
    ActiveObjects::instance().setFixedInteractionDataSource(dataSource);
  }

  void unfixInteractionDataSource()
  {
    ActiveObjects::instance().setFixedInteractionDataSource(nullptr);
  }

  void computeCenter(double* center, DataSource* ds = nullptr)
  {
    if (!ds) {
      ds = dataSource;
    }

    double lengths[3];
    ds->getPhysicalDimensions(lengths);

    for (auto i = 0; i < 3; ++i) {
      center[i] = lengths[i] / 2;
    }
  }

  void updateInteractionCheckboxes()
  {
    ActiveObjects& activeObjects = ActiveObjects::instance();

    bool translate = activeObjects.translationEnabled();
    bool rotate = activeObjects.rotationEnabled();
    bool scale = activeObjects.scalingEnabled();

    ui.interactTranslate->setChecked(translate);
    ui.interactRotate->setChecked(rotate);
    ui.interactScale->setChecked(scale);
  }

  void populateReferenceDataCheckBoxes()
  {
    auto allDataSources = ModuleManager::instance().allDataSourcesDepthFirst();

    // Do not include this data source
    allDataSources.removeAll(dataSource);

    auto* cb = ui.selectedReferenceData;
    cb->clear();

    // Make the first item null
    QVariant firstItemData;
    firstItemData.setValue(static_cast<DataSource*>(nullptr));
    cb->addItem("None", firstItemData);

    auto labels = ModuleManager::createUniqueLabels(allDataSources);
    for (int i = 0; i < allDataSources.size(); ++i) {
      auto* ds = allDataSources[i];
      auto label = labels[i];

      QVariant data;
      data.setValue(ds);
      cb->addItem(label, data);
    }
  }

  DataSource* selectedReferenceData() const
  {
    // We know we can convert to DataSource*, even for the nullptr
    return ui.selectedReferenceData->currentData().value<DataSource*>();
  }

  void onSelectedReferenceDataChanged()
  {
    if (referenceData) {
      restoreReferenceDataPosition();
    }

    referenceData = selectedReferenceData();
    updateReferenceValues();
    saveReferenceDataPosition();
    alignReferenceDataPosition();
    updateReferenceEnableStates();
  }

  void onAlignVoxelsWithReferenceToggled() { updateReferenceEnableStates(); }

  void updateReferenceValues()
  {
    if (!referenceData) {
      return;
    }

    const double* spacing = referenceData->getSpacing();
    int* dimensions = referenceData->imageData()->GetDimensions();

    setReferenceSpacing(spacing);
    setReferenceShape(dimensions);
  }

  void updateReferenceEnableStates()
  {
    bool enableValuesWidget = alignWithReference() && referenceData == nullptr;
    ui.referenceDataValuesWidget->setEnabled(enableValuesWidget);
  }

  void saveReferenceDataPosition()
  {
    if (!referenceData) {
      return;
    }

    referenceData->displayPosition(savedReferencePosition);
  }

  void alignReferenceDataPosition()
  {
    if (!referenceData || !dataSource) {
      return;
    }

    double center[3];
    computeCenter(center, dataSource);

    double referenceCenter[3];
    computeCenter(referenceCenter, referenceData);

    // Find the difference
    double newPosition[3];
    for (int i = 0; i < 3; ++i) {
      newPosition[i] = center[i] - referenceCenter[i];
    }

    referenceData->setDisplayPosition(newPosition);
  }

  void restoreReferenceDataPosition()
  {
    if (!referenceData) {
      return;
    }

    referenceData->setDisplayPosition(savedReferencePosition);
  }
};

ManualManipulationWidget::ManualManipulationWidget(
  Operator* op, vtkSmartPointer<vtkImageData> image, QWidget* p)
  : CustomPythonOperatorWidget(p)
{
  m_internal.reset(new Internal(op, image, this));
}

CustomPythonOperatorWidget* ManualManipulationWidget::New(
  QWidget* p, Operator* op, vtkSmartPointer<vtkImageData> data)
{
  return new ManualManipulationWidget(op, data, p);
}

ManualManipulationWidget::~ManualManipulationWidget() = default;

void ManualManipulationWidget::getValues(QMap<QString, QVariant>& map)
{
  map.insert("scaling", m_internal->scaling());
  map.insert("rotation", m_internal->rotation());
  map.insert("shift", m_internal->voxelShift());
  map.insert("align_with_reference", m_internal->alignWithReference());
  map.insert("reference_spacing", m_internal->referenceSpacing());
  map.insert("reference_shape", m_internal->referenceShape());
}

void ManualManipulationWidget::setValues(const QMap<QString, QVariant>& map)
{
  if (map.contains("scaling")) {
    double scaling[3];
    auto array = map["scaling"].toList();
    for (int i = 0; i < 3; ++i) {
      scaling[i] = array[i].toDouble();
    }
    m_internal->setScaling(scaling);
  }
  if (map.contains("rotation")) {
    double rotation[3];
    auto array = map["rotation"].toList();
    for (int i = 0; i < 3; ++i) {
      rotation[i] = array[i].toDouble();
    }
    m_internal->setRotation(rotation);
  }
  if (map.contains("shift")) {
    int shift[3];
    auto array = map["shift"].toList();
    for (int i = 0; i < 3; ++i) {
      shift[i] = array[i].toInt();
    }
    m_internal->setVoxelShift(shift);
  }

  if (map.contains("align_with_reference")) {
    m_internal->setAlignWithReference(map["align_with_reference"].toBool());
  }

  if (map.contains("reference_spacing")) {
    double referenceSpacing[3];
    auto array = map["reference_spacing"].toList();
    for (int i = 0; i < 3; ++i) {
      referenceSpacing[i] = array[i].toDouble();
    }
    m_internal->setReferenceSpacing(referenceSpacing);
  }

  if (map.contains("reference_shape")) {
    int referenceShape[3];
    auto array = map["reference_shape"].toList();
    for (int i = 0; i < 3; ++i) {
      referenceShape[i] = array[i].toInt();
    }
    m_internal->setReferenceShape(referenceShape);
  }

  m_internal->updateGui();
}

} // namespace tomviz

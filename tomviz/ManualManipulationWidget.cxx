/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ManualManipulationWidget.h"
#include "ui_ManualManipulationWidget.h"

#include "ActiveObjects.h"
#include "InteractiveTransformWidget.h"

#include "pipeline/InputPort.h"
#include "pipeline/Link.h"
#include "pipeline/Node.h"
#include "pipeline/OutputPort.h"
#include "pipeline/Pipeline.h"
#include "pipeline/PortType.h"
#include "pipeline/data/VolumeData.h"
#include "pipeline/sinks/LegacyModuleSink.h"

#include <pqView.h>

#include <vtkActor.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkOutlineSource.h>
#include <vtkPVRenderView.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSMViewProxy.h>
#include <vtkTransform.h>
#include <vtkWeakPointer.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPointer>
#include <QSpinBox>

#include <cmath>

namespace tomviz {

using pipeline::OutputPort;
using pipeline::VolumeDataPtr;

class ManualManipulationWidget::Internal
{
public:
  Ui::ManualManipulationWidget ui;
  vtkSmartPointer<vtkImageData> image;
  ManualManipulationWidget* parent;

  QPointer<pipeline::Node> node;
  QPointer<pipeline::Pipeline> pipeline;

  // The transform being edited. The same prop-style composition the box
  // widget and VolumeSink use: x -> position + R(orientation) * S(scale) * x,
  // rotations applied in vtkProp3D's Z-X-Y order. scale doubles as the
  // output spacing, matching the operator's "scaling" parameter.
  double position[3] = { 0, 0, 0 };
  double orientation[3] = { 0, 0, 0 };
  double scale[3] = { 1, 1, 1 };

  // Live preview: the upstream volume we moved, and the state to put back.
  VolumeDataPtr previewVolume;
  double savedPosition[3] = { 0, 0, 0 };
  double savedOrientation[3] = { 0, 0, 0 };
  double savedSpacing[3] = { 1, 1, 1 };
  bool previewActive = false;

  // Reference data: ports offered in the combo (parallel to items 1..n),
  // and the volume whose display position we borrowed for center-alignment.
  QList<QPointer<OutputPort>> referencePorts;
  VolumeDataPtr referenceVolume;
  QPointer<OutputPort> referencePort;
  double savedReferencePosition[3] = { 0, 0, 0 };

  // Red outline marking the volume's original (untransformed) bounds.
  vtkSmartPointer<vtkActor> outlineActor;
  vtkWeakPointer<vtkRenderer> outlineRenderer;

  Internal(vtkSmartPointer<vtkImageData> img, ManualManipulationWidget* p)
    : image(img), parent(p)
  {
    ui.setupUi(p);

    if (image) {
      double spacing[3];
      image->GetSpacing(spacing);
      for (int i = 0; i < 3; ++i) {
        scale[i] = spacing[i];
      }
    }

    // Interaction needs input data to place the box against.
    ui.interactionGroup->setEnabled(image != nullptr);

    updateGui();
    updateReferenceEnableStates();
    setupConnections();

    // Match the old behavior: interaction is live as soon as the editor
    // opens, so the user can immediately drag the volume into place. If
    // another consumer already holds the box widget, this unchecks
    // itself gracefully.
    if (image) {
      ui.interactTranslate->setChecked(true);
      ui.interactRotate->setChecked(true);
      ui.interactScale->setChecked(true);
      updateTransformWidget();
    }
  }

  ~Internal()
  {
    restorePreview();
    restoreReferencePosition();
    removeOriginalOutline();

    auto& itw = InteractiveTransformWidget::instance();
    if (itw.currentUser() == parent) {
      itw.release(parent);
    }
  }

  void setupConnections()
  {
    auto& itw = InteractiveTransformWidget::instance();

    QObject::connect(ui.interactTranslate, &QCheckBox::clicked, parent,
                     [this]() { updateTransformWidget(); });
    QObject::connect(ui.interactRotate, &QCheckBox::clicked, parent,
                     [this]() { updateTransformWidget(); });
    QObject::connect(ui.interactScale, &QCheckBox::clicked, parent,
                     [this]() { updateTransformWidget(); });

    QObject::connect(&itw, &InteractiveTransformWidget::widgetReleased,
                     parent, [this, &itw]() {
                       if (itw.currentUser() != parent) {
                         ui.interactTranslate->setChecked(false);
                         ui.interactRotate->setChecked(false);
                         ui.interactScale->setChecked(false);
                       }
                     });

    QObject::connect(&itw, &InteractiveTransformWidget::transformChanged,
                     parent,
                     [this](const double* pos, const double* orient,
                            const double* sc) {
                       onBoxTransformChanged(pos, orient, sc);
                     });

    QList<QDoubleSpinBox*> shiftWidgets = { ui.shiftX, ui.shiftY, ui.shiftZ };
    QList<QDoubleSpinBox*> rotateWidgets = { ui.rotateX, ui.rotateY,
                                             ui.rotateZ };
    QList<QDoubleSpinBox*> scaleWidgets = { ui.scaleX, ui.scaleY, ui.scaleZ };

    for (int axis = 0; axis < 3; ++axis) {
      QObject::connect(shiftWidgets[axis], &QDoubleSpinBox::editingFinished,
                       parent, [this, axis, shiftWidgets]() {
                         setShiftValue(axis, shiftWidgets[axis]->value());
                       });
      QObject::connect(rotateWidgets[axis], &QDoubleSpinBox::editingFinished,
                       parent, [this, axis, rotateWidgets]() {
                         setRotationValue(axis, rotateWidgets[axis]->value());
                       });
      QObject::connect(scaleWidgets[axis], &QDoubleSpinBox::editingFinished,
                       parent, [this, axis, scaleWidgets]() {
                         setScalingValue(axis, scaleWidgets[axis]->value());
                       });
    }

    QObject::connect(ui.selectedReferenceData,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     parent, [this]() { onSelectedReferenceDataChanged(); });
    QObject::connect(ui.alignVoxelsWithReference, &QCheckBox::toggled, parent,
                     [this]() { updateReferenceEnableStates(); });
  }

  void setNodeContext(pipeline::Node* n, pipeline::Pipeline* pip)
  {
    node = n;
    pipeline = pip;
    populateReferenceCombo();
  }

  OutputPort* upstreamPort() const
  {
    if (!node) {
      return nullptr;
    }
    auto* in = node->inputPort(QStringLiteral("volume"));
    if (!in || !in->link()) {
      return nullptr;
    }
    return in->link()->from();
  }

  // --- Transform math (matches ManualManipulation.py's conventions) ---

  void computeCenter(double* center) const
  {
    const int* dims = image ? image->GetDimensions() : nullptr;
    for (int i = 0; i < 3; ++i) {
      center[i] = dims ? dims[i] * scale[i] / 2.0 : 0.0;
    }
  }

  // The physical displacement of the volume center. Rotation is about
  // the prop origin, so the center is the natural anchor: the Python
  // script rotates about the center and then applies this shift, which
  // reproduces the prop transform exactly.
  QList<QVariant> physicalShift() const
  {
    double center[3];
    computeCenter(center);

    vtkNew<vtkTransform> t;
    t->Translate(position[0], position[1], position[2]);
    t->RotateZ(orientation[2]);
    t->RotateX(orientation[0]);
    t->RotateY(orientation[1]);

    double moved[3];
    t->TransformPoint(center, moved);

    return { moved[0] - center[0], moved[1] - center[1],
             moved[2] - center[2] };
  }

  QList<QVariant> voxelShift() const
  {
    auto phys = physicalShift();
    int shifts[3];
    for (int i = 0; i < 3; ++i) {
      shifts[i] = (scale[i] > 0)
                    ? static_cast<int>(
                        std::round(phys[i].toDouble() / scale[i]))
                    : 0;
    }
    return { shifts[0], shifts[1], shifts[2] };
  }

  // Solve for the box position that produces @a physShift at the center,
  // keeping the current orientation.
  void setPhysicalShift(const double* physShift)
  {
    double center[3];
    computeCenter(center);

    vtkNew<vtkTransform> t;
    t->RotateZ(orientation[2]);
    t->RotateX(orientation[0]);
    t->RotateY(orientation[1]);

    double rotatedCenter[3];
    t->TransformPoint(center, rotatedCenter);

    for (int i = 0; i < 3; ++i) {
      position[i] = physShift[i] - rotatedCenter[i] + center[i];
    }
  }

  void setVoxelShift(const int* shift)
  {
    double phys[3];
    for (int i = 0; i < 3; ++i) {
      phys[i] = shift[i] * scale[i];
    }
    setPhysicalShift(phys);
  }

  QList<QVariant> scaling() const
  {
    return { scale[0], scale[1], scale[2] };
  }

  QList<QVariant> rotation() const
  {
    return { orientation[0], orientation[1], orientation[2] };
  }

  // --- Spinbox handlers ---

  void setShiftValue(int axis, double value)
  {
    auto prev = physicalShift();
    double phys[3];
    for (int i = 0; i < 3; ++i) {
      phys[i] = prev[i].toDouble();
    }
    phys[axis] = value;
    setPhysicalShift(phys);
    syncBoxAndPreview();
    updateGui();
  }

  void setRotationValue(int axis, double value)
  {
    // Keep the shift fixed while the orientation changes.
    auto prev = physicalShift();
    double phys[3];
    for (int i = 0; i < 3; ++i) {
      phys[i] = prev[i].toDouble();
    }
    orientation[axis] = value;
    setPhysicalShift(phys);
    syncBoxAndPreview();
    updateGui();
  }

  void setScalingValue(int axis, double value)
  {
    scale[axis] = value;
    syncBoxAndPreview();
    updateGui();
  }

  void updateGui()
  {
    auto shifts = physicalShift();

    QList<QDoubleSpinBox*> shiftWidgets = { ui.shiftX, ui.shiftY, ui.shiftZ };
    QList<QDoubleSpinBox*> rotateWidgets = { ui.rotateX, ui.rotateY,
                                             ui.rotateZ };
    QList<QDoubleSpinBox*> scaleWidgets = { ui.scaleX, ui.scaleY, ui.scaleZ };

    for (int i = 0; i < 3; ++i) {
      shiftWidgets[i]->setValue(shifts[i].toDouble());
      rotateWidgets[i]->setValue(orientation[i]);
      scaleWidgets[i]->setValue(scale[i]);
    }
  }

  // --- Interactive box widget ---

  void updateTransformWidget()
  {
    auto& itw = InteractiveTransformWidget::instance();
    bool translate = ui.interactTranslate->isChecked();
    bool rotate = ui.interactRotate->isChecked();
    bool doScale = ui.interactScale->isChecked();
    bool anyEnabled = translate || rotate || doScale;

    if (!anyEnabled) {
      if (itw.currentUser() == parent) {
        itw.release(parent);
      }
      return;
    }

    bool freshAcquire = (itw.currentUser() != parent);
    if (freshAcquire && !itw.acquire(parent)) {
      // Another consumer (e.g. the volume properties panel) holds it.
      ui.interactTranslate->setChecked(false);
      ui.interactRotate->setChecked(false);
      ui.interactScale->setChecked(false);
      return;
    }

    if (freshAcquire) {
      itw.setView(ActiveObjects::instance().activePqView());
      placeBox();
      showOriginalOutline();
    }

    itw.setTranslationEnabled(translate);
    itw.setRotationEnabled(rotate);
    itw.setScalingEnabled(doScale);
  }

  void placeBox()
  {
    if (!image) {
      return;
    }

    auto& itw = InteractiveTransformWidget::instance();

    int ext[6];
    image->GetExtent(ext);
    double bounds[6];
    for (int i = 0; i < 6; ++i) {
      bounds[i] = static_cast<double>(ext[i]);
    }
    itw.setBounds(bounds);
    itw.setTransform(position, orientation, scale);
  }

  void onBoxTransformChanged(const double* pos, const double* orient,
                             const double* sc)
  {
    auto& itw = InteractiveTransformWidget::instance();
    if (itw.currentUser() != parent) {
      return;
    }

    for (int i = 0; i < 3; ++i) {
      position[i] = pos[i];
      orientation[i] = orient[i];
      scale[i] = sc[i];
    }

    updateGui();
    applyPreview();
  }

  void syncBox()
  {
    auto& itw = InteractiveTransformWidget::instance();
    if (itw.currentUser() == parent) {
      itw.setTransform(position, orientation, scale);
    }
  }

  void syncBoxAndPreview()
  {
    syncBox();
    applyPreview();
  }

  // --- Live preview on the upstream volume ---

  void applyPreview()
  {
    auto* port = upstreamPort();
    if (!port || !port->hasData()) {
      return;
    }
    auto vol = port->data().value<VolumeDataPtr>();
    if (!vol || !vol->isValid()) {
      return;
    }

    if (!previewActive || vol != previewVolume) {
      // First touch of this volume: remember what to put back.
      if (previewActive && vol != previewVolume) {
        restorePreview();
      }
      previewVolume = vol;
      auto p = vol->displayPosition();
      auto o = vol->displayOrientation();
      auto s = vol->spacing();
      for (int i = 0; i < 3; ++i) {
        savedPosition[i] = p[i];
        savedOrientation[i] = o[i];
        savedSpacing[i] = s[i];
      }
      previewActive = true;
    }

    vol->setDisplayPosition(position[0], position[1], position[2]);
    vol->setDisplayOrientation(orientation[0], orientation[1],
                               orientation[2]);
    vol->setSpacing(scale[0], scale[1], scale[2]);
    emit port->metadataChanged();
    notifyPreviewConsumers();
  }

  // Spacing lives on the image itself, so a re-render alone shows it,
  // but display position and orientation are applied by each sink to
  // its own prop when its port reports metadata changes. A sink does
  // not necessarily listen on the port we just mutated (it latches the
  // connection to whatever port fed its first run, which a later
  // insertion can change), so tell every sink showing this exact
  // volume directly.
  void notifyPreviewConsumers()
  {
    if (!pipeline || !previewVolume) {
      return;
    }
    for (auto* n : pipeline->nodes()) {
      auto* sink = qobject_cast<pipeline::LegacyModuleSink*>(n);
      if (sink && sink->volumeData() == previewVolume) {
        sink->onMetadataChanged();
      }
    }
  }

  void restorePreview()
  {
    if (!previewActive) {
      return;
    }
    previewActive = false;

    if (previewVolume && previewVolume->isValid()) {
      previewVolume->setDisplayPosition(savedPosition[0], savedPosition[1],
                                        savedPosition[2]);
      previewVolume->setDisplayOrientation(
        savedOrientation[0], savedOrientation[1], savedOrientation[2]);
      previewVolume->setSpacing(savedSpacing[0], savedSpacing[1],
                                savedSpacing[2]);
    }
    notifyPreviewConsumers();
    previewVolume.reset();

    if (auto* port = upstreamPort()) {
      emit port->metadataChanged();
    }
    render();
  }

  // --- Original-position outline ---

  void showOriginalOutline()
  {
    if (outlineActor || !image) {
      return;
    }

    auto* view = ActiveObjects::instance().activeView();
    if (!view) {
      return;
    }
    auto* renderView =
      vtkPVRenderView::SafeDownCast(view->GetClientSideView());
    if (!renderView || !renderView->GetRenderer()) {
      return;
    }

    int ext[6];
    double spacing[3];
    image->GetExtent(ext);
    // The preview mutates the image's spacing in place, so once a
    // preview has started the original spacing lives in savedSpacing.
    if (previewActive) {
      for (int i = 0; i < 3; ++i) {
        spacing[i] = savedSpacing[i];
      }
    } else {
      image->GetSpacing(spacing);
    }

    double bounds[6];
    for (int i = 0; i < 3; ++i) {
      bounds[2 * i] = ext[2 * i] * spacing[i];
      bounds[2 * i + 1] = ext[2 * i + 1] * spacing[i];
    }

    vtkNew<vtkOutlineSource> outline;
    outline->SetBounds(bounds);
    outline->Update();

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(outline->GetOutputPort());

    outlineActor = vtkSmartPointer<vtkActor>::New();
    outlineActor->SetMapper(mapper);
    outlineActor->GetProperty()->SetColor(1, 0, 0);
    outlineActor->GetProperty()->SetLineWidth(5);
    outlineActor->PickableOff();

    outlineRenderer = renderView->GetRenderer();
    outlineRenderer->AddActor(outlineActor);
    render();
  }

  void removeOriginalOutline()
  {
    if (outlineActor && outlineRenderer) {
      outlineRenderer->RemoveActor(outlineActor);
    }
    outlineActor = nullptr;
    outlineRenderer = nullptr;
    render();
  }

  void render()
  {
    if (auto* view = ActiveObjects::instance().activePqView()) {
      view->render();
    }
  }

  // --- Reference data ---

  void populateReferenceCombo()
  {
    auto* cb = ui.selectedReferenceData;
    QSignalBlocker blocker(cb);

    cb->clear();
    referencePorts.clear();
    cb->addItem(QStringLiteral("None"));

    if (!pipeline) {
      return;
    }

    auto* ourUpstream = upstreamPort();
    for (auto* n : pipeline->nodes()) {
      if (n == node) {
        continue;
      }
      auto ports = n->outputPorts();
      for (auto* port : ports) {
        if (port == ourUpstream || !port->hasData() ||
            !pipeline::isVolumeType(port->type())) {
          continue;
        }
        auto vol = port->data().value<VolumeDataPtr>();
        if (!vol || !vol->isValid()) {
          continue;
        }
        QString label = n->label();
        if (ports.size() > 1) {
          label += QStringLiteral(" / ") + port->name();
        }
        cb->addItem(label);
        referencePorts.append(port);
      }
    }
  }

  void onSelectedReferenceDataChanged()
  {
    restoreReferencePosition();

    int index = ui.selectedReferenceData->currentIndex();
    OutputPort* port =
      (index >= 1 && index <= referencePorts.size())
        ? referencePorts[index - 1].data()
        : nullptr;

    if (port && port->hasData()) {
      auto vol = port->data().value<VolumeDataPtr>();
      if (vol && vol->isValid()) {
        referenceVolume = vol;
        referencePort = port;

        auto spacing = vol->spacing();
        auto dims = vol->dimensions();
        QList<QDoubleSpinBox*> spacingWidgets = { ui.referenceSpacingX,
                                                  ui.referenceSpacingY,
                                                  ui.referenceSpacingZ };
        QList<QSpinBox*> shapeWidgets = { ui.referenceShapeX,
                                          ui.referenceShapeY,
                                          ui.referenceShapeZ };
        for (int i = 0; i < 3; ++i) {
          spacingWidgets[i]->setValue(spacing[i]);
          shapeWidgets[i]->setValue(dims[i]);
        }

        auto p = vol->displayPosition();
        for (int i = 0; i < 3; ++i) {
          savedReferencePosition[i] = p[i];
        }
        alignReferenceDataPosition();
      }
    }

    updateReferenceEnableStates();
  }

  // Center the reference volume on the volume being manipulated, so the
  // user can align them visually.
  void alignReferenceDataPosition()
  {
    if (!referenceVolume || !referenceVolume->isValid()) {
      return;
    }

    double center[3];
    computeCenter(center);

    auto refSpacing = referenceVolume->spacing();
    auto refDims = referenceVolume->dimensions();

    double newPosition[3];
    for (int i = 0; i < 3; ++i) {
      double refCenter = refDims[i] * refSpacing[i] / 2.0;
      newPosition[i] = center[i] - refCenter;
    }

    referenceVolume->setDisplayPosition(newPosition[0], newPosition[1],
                                        newPosition[2]);
    if (referencePort) {
      emit referencePort->metadataChanged();
    }
    render();
  }

  void restoreReferencePosition()
  {
    if (!referenceVolume) {
      return;
    }

    if (referenceVolume->isValid()) {
      referenceVolume->setDisplayPosition(savedReferencePosition[0],
                                          savedReferencePosition[1],
                                          savedReferencePosition[2]);
    }
    referenceVolume.reset();

    if (referencePort) {
      emit referencePort->metadataChanged();
      referencePort = nullptr;
    }
    render();
  }

  void updateReferenceEnableStates()
  {
    // Manual spacing/shape entry only makes sense when aligning without
    // a selected reference dataset.
    bool enableValuesWidget =
      ui.alignVoxelsWithReference->isChecked() && !referenceVolume;
    ui.referenceDataValuesWidget->setEnabled(enableValuesWidget);
  }

  QList<QVariant> referenceSpacing() const
  {
    return { ui.referenceSpacingX->value(), ui.referenceSpacingY->value(),
             ui.referenceSpacingZ->value() };
  }

  QList<QVariant> referenceShape() const
  {
    return { ui.referenceShapeX->value(), ui.referenceShapeY->value(),
             ui.referenceShapeZ->value() };
  }
};

ManualManipulationWidget::ManualManipulationWidget(
  const QMap<QString, pipeline::PortData>& inputs, QWidget* parent)
  : CustomPythonNodeWidget(parent)
{
  vtkSmartPointer<vtkImageData> image;
  if (auto it = inputs.constFind(QStringLiteral("volume"));
      it != inputs.constEnd()) {
    if (auto vol = it.value().value<pipeline::VolumeDataPtr>();
        vol && vol->isValid()) {
      image = vol->imageData();
    }
  }
  m_internal.reset(new Internal(image, this));
}

ManualManipulationWidget::~ManualManipulationWidget() = default;

void ManualManipulationWidget::setNodeContext(pipeline::Node* node,
                                              pipeline::Pipeline* pipeline)
{
  m_internal->setNodeContext(node, pipeline);
}

void ManualManipulationWidget::writeSettings()
{
  // Apply is about to bake the transform into the data. Put the preview
  // back so the result is not transformed twice, and let the reference
  // volume return to where it was.
  m_internal->restorePreview();
  m_internal->restoreReferencePosition();
}

void ManualManipulationWidget::getValues(QMap<QString, QVariant>& map)
{
  map.insert("scaling", m_internal->scaling());
  map.insert("rotation", m_internal->rotation());
  map.insert("shift", m_internal->voxelShift());
  map.insert("align_with_reference",
             m_internal->ui.alignVoxelsWithReference->isChecked());
  map.insert("reference_spacing", m_internal->referenceSpacing());
  map.insert("reference_shape", m_internal->referenceShape());
}

void ManualManipulationWidget::setValues(const QMap<QString, QVariant>& map)
{
  // Order matters: scaling feeds the center used by the shift solve, and
  // rotation feeds the same solve, so restore scaling and rotation first.
  if (map.contains("scaling")) {
    auto array = map["scaling"].toList();
    if (array.size() >= 3) {
      for (int i = 0; i < 3; ++i) {
        double v = array[i].toDouble();
        if (v > 0) {
          m_internal->scale[i] = v;
        }
      }
    }
  }

  if (map.contains("rotation")) {
    auto array = map["rotation"].toList();
    if (array.size() >= 3) {
      for (int i = 0; i < 3; ++i) {
        m_internal->orientation[i] = array[i].toDouble();
      }
    }
  }

  if (map.contains("shift")) {
    auto array = map["shift"].toList();
    if (array.size() >= 3) {
      int shift[3] = { array[0].toInt(), array[1].toInt(),
                       array[2].toInt() };
      m_internal->setVoxelShift(shift);
    }
  }

  if (map.contains("align_with_reference")) {
    m_internal->ui.alignVoxelsWithReference->setChecked(
      map["align_with_reference"].toBool());
  }

  if (map.contains("reference_spacing")) {
    auto array = map["reference_spacing"].toList();
    if (array.size() >= 3) {
      QList<QDoubleSpinBox*> widgets = { m_internal->ui.referenceSpacingX,
                                         m_internal->ui.referenceSpacingY,
                                         m_internal->ui.referenceSpacingZ };
      for (int i = 0; i < 3; ++i) {
        widgets[i]->setValue(array[i].toDouble());
      }
    }
  }

  if (map.contains("reference_shape")) {
    auto array = map["reference_shape"].toList();
    if (array.size() >= 3) {
      QList<QSpinBox*> widgets = { m_internal->ui.referenceShapeX,
                                   m_internal->ui.referenceShapeY,
                                   m_internal->ui.referenceShapeZ };
      for (int i = 0; i < 3; ++i) {
        widgets[i]->setValue(array[i].toInt());
      }
    }
  }

  m_internal->updateGui();

  // Loading values must not start a preview by itself: an already-applied
  // node shows its baked output, and moving the upstream volume here would
  // display the transform twice. Only refresh what's already live.
  m_internal->syncBox();
  if (m_internal->previewActive) {
    m_internal->applyPreview();
  }
}

} // namespace tomviz

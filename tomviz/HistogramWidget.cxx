/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "HistogramWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "DoubleSliderWidget.h"
#include "ModuleContour.h"
#include "ModuleManager.h"
#include "PresetDialog.h"
#include "QVTKGLWidget.h"
#include "Utilities.h"

#include "vtkChartHistogramColorOpacityEditor.h"

#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkControlPointsItem.h>
#include <vtkDataArray.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkTable.h>
#include <vtkVector.h>
#include <vtk_jsoncpp.h>

#include <pqApplicationCore.h>
#include <pqPresetDialog.h>
#include <pqResetScalarRangeReaction.h>
#include <pqServerManagerModel.h>
#include <pqSettings.h>
#include <pqView.h>

#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMTransferFunctionManager.h>
#include <vtkSMTransferFunctionPresets.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QToolButton>
#include <QVBoxLayout>

#include <QDebug>

namespace tomviz {

HistogramWidget::HistogramWidget(QWidget* parent)
  : QWidget(parent), m_qvtk(new QVTKGLWidget(this))
{
  // Set up our little chart.
  m_histogramView->SetRenderWindow(m_qvtk->renderWindow());
  m_histogramView->SetInteractor(m_qvtk->interactor());
  m_histogramView->GetScene()->AddItem(m_histogramColorOpacityEditor.Get());

  // Connect events from the histogram color/opacity editor.
  m_eventLink->Connect(m_histogramColorOpacityEditor.Get(),
                       vtkCommand::CursorChangedEvent, this,
                       SLOT(histogramClicked(vtkObject*)));
  m_eventLink->Connect(m_histogramColorOpacityEditor.Get(),
                       vtkCommand::EndEvent, this,
                       SLOT(onScalarOpacityFunctionChanged()));
  m_eventLink->Connect(m_histogramColorOpacityEditor.Get(),
                       vtkControlPointsItem::CurrentPointEditEvent, this,
                       SLOT(onCurrentPointEditEvent()));

  auto hLayout = new QHBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  auto vLayout = new QVBoxLayout;
  hLayout->addLayout(vLayout);
  hLayout->setContentsMargins(0, 0, 5, 0);

  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->addStretch(1);

  auto button = new QToolButton;
  button->setIcon(QIcon(":/icons/pqResetRange.png"));
  button->setToolTip("Reset data range");
  connect(button, SIGNAL(clicked()), this, SLOT(onResetRangeClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/icons/pqResetRangeCustom.png"));
  button->setToolTip("Specify data range");
  connect(button, SIGNAL(clicked()), this, SLOT(onCustomRangeClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/icons/pqInvert.png"));
  button->setToolTip("Invert color map");
  connect(button, SIGNAL(clicked()), this, SLOT(onInvertClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/icons/pqFavorites.png"));
  button->setToolTip("Choose preset color map");
  connect(button, SIGNAL(clicked()), this, SLOT(onPresetClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqSave16.png"));
  button->setToolTip("Save current color map as a preset");
  connect(button, SIGNAL(clicked()), this, SLOT(onSaveToPresetClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  m_colorLegendToolButton = button;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqScalarBar24.png"));
  button->setToolTip("Show color legend in the 3D window.");
  button->setEnabled(false);
  button->setCheckable(true);
  connect(button, SIGNAL(toggled(bool)), this,
          SIGNAL(colorLegendToggled(bool)));
  button->setChecked(false);
  vLayout->addWidget(button);

  vLayout->addStretch(1);

  connect(&ActiveObjects::instance(), SIGNAL(viewChanged(vtkSMViewProxy*)),
          this, SLOT(updateUI()));
  connect(this, SIGNAL(colorMapUpdated()), this, SLOT(updateUI()));

  setLayout(hLayout);
}

HistogramWidget::~HistogramWidget() = default;

void HistogramWidget::setLUT(vtkPVDiscretizableColorTransferFunction* lut)
{
  if (m_LUT != lut) {
    if (m_scalarOpacityFunction) {
      m_eventLink->Disconnect(m_scalarOpacityFunction,
                              vtkCommand::ModifiedEvent, this,
                              SLOT(onScalarOpacityFunctionChanged()));
    }
    m_LUT = lut;
    m_scalarOpacityFunction = m_LUT->GetScalarOpacityFunction();
    m_eventLink->Connect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent,
                         this, SLOT(onScalarOpacityFunctionChanged()));
    emit colorMapUpdated();
  }
}

void HistogramWidget::setLUTProxy(vtkSMProxy* proxy)
{
  if (proxy && m_LUTProxy != proxy) {
    m_LUTProxy = proxy;
    vtkPVDiscretizableColorTransferFunction* lut =
      vtkPVDiscretizableColorTransferFunction::SafeDownCast(
        proxy->GetClientSideObject());
    setLUT(lut);

    auto view = ActiveObjects::instance().activeView();

    // Update widget to reflect scalar bar visibility.
    if (m_LUTProxy) {
      auto sbProxy = getScalarBarRepresentation(view);
      if (sbProxy) {
        bool visible =
          vtkSMPropertyHelper(sbProxy, "Visibility").GetAsInt() == 1;
        m_colorLegendToolButton->setChecked(visible);
      }
    }
  }
}

void HistogramWidget::setInputData(vtkTable* table, const char* x_,
                                   const char* y_)
{
  m_inputData = table;
  m_histogramColorOpacityEditor->SetHistogramInputData(table, x_, y_);
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  if (m_LUT && table) {
    m_histogramColorOpacityEditor->SetScalarVisibility(true);
    m_histogramColorOpacityEditor->SetColorTransferFunction(m_LUT);
    m_histogramColorOpacityEditor->SelectColorArray("image_extents");
  }
  m_histogramView->Render();
}

vtkSMProxy* HistogramWidget::getScalarBarRepresentation(vtkSMProxy* view)
{
  if (!view) {
    return nullptr;
  }

  vtkSMTransferFunctionProxy* tferProxy =
    vtkSMTransferFunctionProxy::SafeDownCast(m_LUTProxy);
  if (!tferProxy) {
    return nullptr;
  }

  auto sbProxy = tferProxy->FindScalarBarRepresentation(view);
  if (!sbProxy) {
    // No scalar bar representation exists yet, create it and initialize it
    // with some default settings.
    vtkNew<vtkSMTransferFunctionManager> tferManager;
    sbProxy = tferManager->GetScalarBarRepresentation(m_LUTProxy, view);
    vtkSMPropertyHelper(sbProxy, "Visibility").Set(0);
    vtkSMPropertyHelper(sbProxy, "Enabled").Set(0);
    vtkSMPropertyHelper(sbProxy, "Title").Set("");
    vtkSMPropertyHelper(sbProxy, "ComponentTitle").Set("");
    vtkSMPropertyHelper(sbProxy, "RangeLabelFormat").Set("%g");
    sbProxy->UpdateVTKObjects();
  }

  return sbProxy;
}

void HistogramWidget::onScalarOpacityFunctionChanged()
{
  auto core = pqApplicationCore::instance();
  auto smModel = core->getServerManagerModel();
  QList<pqView*> views = smModel->findItems<pqView*>();
  foreach (pqView* view, views) {
    view->render();
  }

  // Update the histogram
  m_histogramView->GetRenderWindow()->Render();

  // Update the scalar opacity function proxy as it does not update it's
  // internal state when the VTK object changes.
  if (!m_LUTProxy) {
    return;
  }

  vtkSMProxy* opacityMapProxy =
    vtkSMPropertyHelper(m_LUTProxy, "ScalarOpacityFunction", true).GetAsProxy();
  if (!opacityMapProxy) {
    return;
  }

  vtkSMPropertyHelper pointsHelper(opacityMapProxy, "Points");
  vtkObjectBase* opacityMapObject = opacityMapProxy->GetClientSideObject();
  auto pwf = vtkPiecewiseFunction::SafeDownCast(opacityMapObject);
  if (pwf) {
    pointsHelper.SetNumberOfElements(4 * pwf->GetSize());
    for (int i = 0; i < pwf->GetSize(); ++i) {
      double value[4];
      pwf->GetNodeValue(i, value);
      pointsHelper.Set(4 * i + 0, value[0]);
      pointsHelper.Set(4 * i + 1, value[1]);
      pointsHelper.Set(4 * i + 2, value[2]);
      pointsHelper.Set(4 * i + 3, value[3]);
    }
  }

  emit opacityChanged();
}

void HistogramWidget::onCurrentPointEditEvent()
{
  double rgb[3];
  if (m_histogramColorOpacityEditor->GetCurrentControlPointColor(rgb)) {
    QColor color =
      QColorDialog::getColor(QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), this,
                             "Select Color for Control Point");
    if (color.isValid()) {
      rgb[0] = color.redF();
      rgb[1] = color.greenF();
      rgb[2] = color.blueF();
      m_histogramColorOpacityEditor->SetCurrentControlPointColor(rgb);
      onScalarOpacityFunctionChanged();
    }
  }
}

void HistogramWidget::histogramClicked(vtkObject*)
{
  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  Q_ASSERT(activeDataSource);

  auto view = ActiveObjects::instance().activeView();
  if (!view) {
    return;
  }

  // Use active ModuleContour is possible. Otherwise, find the first existing
  // ModuleContour instance or just create a new one, if none exists.
  typedef ModuleContour ModuleContourType;

  auto isoValue = m_histogramColorOpacityEditor->GetContourValue();
  auto contour =
    qobject_cast<ModuleContourType*>(ActiveObjects::instance().activeModule());
  if (!contour) {
    QList<ModuleContourType*> contours =
      ModuleManager::instance().findModules<ModuleContourType*>(
        activeDataSource, view);
    if (contours.size() == 0) {
      auto res = createContourDialog(isoValue);
      if (!res) {
        return;
      }
      contour = qobject_cast<ModuleContourType*>(
        ModuleManager::instance().createAndAddModule("Contour",
                                                     activeDataSource, view));
    } else {
      contour = contours[0];
    }
    ActiveObjects::instance().setActiveModule(contour);
  }
  Q_ASSERT(contour);
  contour->setIsoValue(isoValue);
  tomviz::convert<pqView*>(view)->render();
}

bool HistogramWidget::createContourDialog(double& isoValue)
{
  QSettings* settings = pqApplicationCore::instance()->settings();
  bool autoAccept =
    settings->value("ContourSettings.AutoAccept", false).toBool();
  if (autoAccept) {
    return true;
  }

  auto ds = ActiveObjects::instance().activeDataSource();
  if (!ds) {
    return false;
  }

  QDialog dialog;
  dialog.setFixedWidth(300);
  dialog.setMaximumHeight(50);
  dialog.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  QVBoxLayout vLayout;
  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("New Iso Contour"));

  QFormLayout formLayout;
  vLayout.addLayout(&formLayout);

  // Get the range of the dataset
  double range[2];
  ds->getRange(range);

  DoubleSliderWidget w(true);
  w.setMinimum(range[0]);
  w.setMaximum(range[1]);

  // We want to round this to two decimal places
  isoValue = QString::number(isoValue, 'f', 2).toDouble();
  w.setValue(isoValue);

  w.setLineEditWidth(50);

  formLayout.addRow("Iso value", &w);

  QCheckBox dontAskAgain("Don't ask again");
  formLayout.addRow(&dontAskAgain);

  QDialogButtonBox buttons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
  vLayout.addWidget(&buttons);

  connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  auto r = dialog.exec();

  if (r == QDialog::Accepted) {
    if (dontAskAgain.isChecked()) {
      settings->setValue("ContourSettings.AutoAccept", true);
    }
    isoValue = w.value();
    return true;
  } else {
    return false;
  }
}

void HistogramWidget::onResetRangeClicked()
{
  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  if (!activeDataSource)
    return;

  double range[2];
  activeDataSource->getRange(range);

  rescaleTransferFunction(m_LUTProxy, range[0], range[1]);
  renderViews();
}

void HistogramWidget::onCustomRangeClicked()
{
  // Get the max allowable range
  auto activeDataSource = ActiveObjects::instance().activeDataSource();
  if (!activeDataSource)
    return;

  double maxRange[2];
  activeDataSource->getRange(maxRange);

  // Get the current range
  vtkVector2d currentRange;
  vtkDiscretizableColorTransferFunction* discFunc =
    vtkDiscretizableColorTransferFunction::SafeDownCast(
      m_LUTProxy->GetClientSideObject());
  if (!discFunc) {
    return;
  }
  discFunc->GetRange(currentRange.GetData());

  QDialog dialog;
  QVBoxLayout vLayout;
  QHBoxLayout hLayout;
  vLayout.addLayout(&hLayout);

  // Fix the size of this window
  vLayout.setSizeConstraint(QLayout::SetFixedSize);
  hLayout.setSizeConstraint(QLayout::SetFixedSize);

  dialog.setLayout(&vLayout);
  dialog.setWindowTitle(tr("Specify Data Range"));

  QDoubleSpinBox bottom;
  bottom.setRange(maxRange[0], maxRange[1]);
  bottom.setValue(currentRange[0]);
  bottom.setFixedSize(bottom.sizeHint());
  bottom.setToolTip("Min: " + QString::number(maxRange[0]));
  hLayout.addWidget(&bottom);

  QLabel dash("-");
  dash.setAlignment(Qt::AlignHCenter);
  dash.setAlignment(Qt::AlignVCenter);
  hLayout.addWidget(&dash);

  QDoubleSpinBox top;
  top.setRange(maxRange[0], maxRange[1]);
  top.setValue(currentRange[1]);
  top.setFixedSize(top.sizeHint());
  top.setToolTip("Max: " + QString::number(maxRange[1]));
  hLayout.addWidget(&top);

  // Make sure the bottom isn't higher than the top, and the
  // top isn't higher than the bottom.
  connect(&bottom,
          static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
          &top, &QDoubleSpinBox::setMinimum);
  connect(&top,
          static_cast<void (QDoubleSpinBox::*)(double)>(
            &QDoubleSpinBox::valueChanged),
          &bottom, &QDoubleSpinBox::setMaximum);

  QDialogButtonBox buttonBox;
  buttonBox.addButton(QDialogButtonBox::Ok);
  buttonBox.addButton(QDialogButtonBox::Cancel);
  connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  vLayout.addWidget(&buttonBox);

  if (dialog.exec() == QDialog::Accepted) {
    rescaleTransferFunction(m_LUTProxy, bottom.value(), top.value());
    renderViews();
  }

  // vLayout should not call 'delete' on hLayout...
  hLayout.setParent(nullptr);
}

void HistogramWidget::onInvertClicked()
{
  vtkSMTransferFunctionProxy::InvertTransferFunction(m_LUTProxy);
  renderViews();
  emit colorMapUpdated();
}

void HistogramWidget::showPresetDialog(const char* presetName)
{
  if (m_presetDialog == nullptr) {
    m_presetDialog = new PresetDialog(this);
    QObject::connect(m_presetDialog, &PresetDialog::applyPreset, this,
                     &HistogramWidget::applyCurrentPreset);
  }

  if (presetName) {
    // m_presetDialog->setCurrentPreset(presetName);
  }

  m_presetDialog->show();
}

void HistogramWidget::onSaveToPresetClicked()
{
  vtkSMProxy* lut = m_LUTProxy;
  vtkSMProxy* sof =
    vtkSMPropertyHelper(lut, "ScalarOpacityFunction", true).GetAsProxy();

  Json::Value preset = vtkSMTransferFunctionProxy::GetStateAsPreset(lut);
  Json::Value opacityInfo = vtkSMTransferFunctionProxy::GetStateAsPreset(sof);
  preset["Points"] = opacityInfo["Points"];

  std::string presetName;
  {
    // This scoping is necessary to ensure that the vtkSMTransferFunctionPresets
    // saves the new preset to the "settings" before the choosePreset dialog is
    // shown.
    vtkNew<vtkSMTransferFunctionPresets> presets;
    presetName = presets->AddUniquePreset(preset);
  }

  showPresetDialog(presetName.c_str());
}

void HistogramWidget::onPresetClicked()
{
  showPresetDialog(nullptr);
}

void HistogramWidget::applyCurrentPreset()
{
  vtkSMProxy* lut = m_LUTProxy;

  if (!lut) {
    return;
  }

  QString result = m_presetDialog->presetName();
  vtkSMTransferFunctionProxy::ApplyPreset(lut, result.toLatin1().data(), true);

  renderViews();
  emit colorMapUpdated();
}

void HistogramWidget::updateUI()
{
  auto view = ActiveObjects::instance().activeView();

  // Update widget to reflect scalar bar visibility.
  if (m_LUTProxy) {
    auto sbProxy = getScalarBarRepresentation(view);
    if (view && sbProxy) {
      m_colorLegendToolButton->blockSignals(true);
      m_colorLegendToolButton->setEnabled(true);
      m_colorLegendToolButton->setChecked(
        vtkSMPropertyHelper(sbProxy, "Visibility").GetAsInt() == 1);
      m_colorLegendToolButton->blockSignals(false);
    }
  }
}

void HistogramWidget::renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}

void HistogramWidget::rescaleTransferFunction(vtkSMProxy* lutProxy, double min,
                                              double max)
{
  vtkSMTransferFunctionProxy::RescaleTransferFunction(lutProxy, min, max);
  auto opacityMap =
    vtkSMPropertyHelper(m_LUTProxy, "ScalarOpacityFunction").GetAsProxy();
  vtkSMTransferFunctionProxy::RescaleTransferFunction(opacityMap, min, max);
  emit colorMapUpdated();
}

void HistogramWidget::showEvent(QShowEvent* event)
{
  QWidget::showEvent(event);
  this->renderViews();
}
} // namespace tomviz

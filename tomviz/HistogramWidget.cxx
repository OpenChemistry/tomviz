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
#include "HistogramWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleContour.h"
#include "ModuleManager.h"
#include "Utilities.h"

#include "vtkChartHistogramColorOpacityEditor.h"

#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkControlPointsItem.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkVector.h>

#include <QVTKOpenGLWidget.h>

#include <pqApplicationCore.h>
#include <pqCoreUtilities.h>
#include <pqPresetDialog.h>
#include <pqRescaleRange.h>
#include <pqResetScalarRangeReaction.h>
#include <pqServerManagerModel.h>
#include <pqView.h>

#include <vtkPVDiscretizableColorTransferFunction.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMTransferFunctionProxy.h>
#include <vtkSMViewProxy.h>

#include <QColorDialog>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <QDebug>

namespace tomviz {

HistogramWidget::HistogramWidget(QWidget* parent)
  : QWidget(parent), m_qvtk(new QVTKOpenGLWidget(this))
{
  // Set up our little chart.
  vtkNew<vtkGenericOpenGLRenderWindow> window;
  m_qvtk->SetRenderWindow(window.Get());
  QSurfaceFormat glFormat = QVTKOpenGLWidget::defaultFormat();
  glFormat.setSamples(8);
  m_qvtk->setFormat(glFormat);
  m_histogramView->SetRenderWindow(window.Get());
  m_histogramView->SetInteractor(m_qvtk->GetInteractor());
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
  button->setIcon(QIcon(":/pqWidgets/Icons/pqResetRange24.png"));
  button->setToolTip("Reset data range");
  connect(button, SIGNAL(clicked()), this, SLOT(onResetRangeClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqResetRangeCustom24.png"));
  button->setToolTip("Specify data range");
  connect(button, SIGNAL(clicked()), this, SLOT(onCustomRangeClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqInvert24.png"));
  button->setToolTip("Invert color map");
  connect(button, SIGNAL(clicked()), this, SLOT(onInvertClicked()));
  vLayout->addWidget(button);

  button = new QToolButton;
  button->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites16.png"));
  button->setToolTip("Choose preset color map");
  connect(button, SIGNAL(clicked()), this, SLOT(onPresetClicked()));
  vLayout->addWidget(button);

  m_gradientOpacityButton = new QToolButton;
  m_gradientOpacityButton->setCheckable(true);
  m_gradientOpacityButton->setEnabled(false);
  m_gradientOpacityButton->setIcon(QIcon(":/icons/gradient_opacity.png"));
  m_gradientOpacityButton->setToolTip("Show/Hide Gradient Opacity");
  connect(m_gradientOpacityButton, SIGNAL(toggled(bool)), this,
          SIGNAL(gradientVisibilityChanged(bool)));
  vLayout->addWidget(m_gradientOpacityButton);

  vLayout->addStretch(1);

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
  }
}

void HistogramWidget::setLUTProxy(vtkSMProxy* proxy)
{
  if (m_LUTProxy != proxy) {
    m_LUTProxy = proxy;
    vtkPVDiscretizableColorTransferFunction* lut =
      vtkPVDiscretizableColorTransferFunction::SafeDownCast(
        proxy->GetClientSideObject());
    setLUT(lut);
  }
}

void HistogramWidget::setInputData(vtkTable* table, const char* x_,
                                   const char* y_)
{
  m_histogramColorOpacityEditor->SetHistogramInputData(table, x_, y_);
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  if (m_LUT) {
    m_histogramColorOpacityEditor->SetScalarVisibility(true);
    m_histogramColorOpacityEditor->SetColorTransferFunction(m_LUT);
    m_histogramColorOpacityEditor->SelectColorArray("image_extents");
  }
  m_histogramView->Render();
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
}

void HistogramWidget::onCurrentPointEditEvent()
{
  double rgb[3];
  if (m_histogramColorOpacityEditor->GetCurrentControlPointColor(rgb)) {
    QColor color = QColorDialog::getColor(
      QColor::fromRgbF(rgb[0], rgb[1], rgb[2]), this,
      "Select Color for Control Point", QColorDialog::DontUseNativeDialog);
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

  auto contour =
    qobject_cast<ModuleContourType*>(ActiveObjects::instance().activeModule());
  if (!contour) {
    QList<ModuleContourType*> contours =
      ModuleManager::instance().findModules<ModuleContourType*>(
        activeDataSource, view);
    if (contours.size() == 0) {
      contour = qobject_cast<ModuleContourType*>(
        ModuleManager::instance().createAndAddModule("Contour",
                                                     activeDataSource, view));
    } else {
      contour = contours[0];
    }
    ActiveObjects::instance().setActiveModule(contour);
  }
  Q_ASSERT(contour);
  contour->setIsoValue(m_histogramColorOpacityEditor->GetContourValue());
  tomviz::convert<pqView*>(view)->render();
}

void HistogramWidget::onResetRangeClicked()
{
  pqResetScalarRangeReaction::resetScalarRangeToData(nullptr);
}

void HistogramWidget::onCustomRangeClicked()
{
  vtkVector2d range;
  vtkDiscretizableColorTransferFunction* discFunc =
    vtkDiscretizableColorTransferFunction::SafeDownCast(
      m_LUTProxy->GetClientSideObject());
  if (!discFunc) {
    return;
  }
  discFunc->GetRange(range.GetData());
  pqRescaleRange dialog(pqCoreUtilities::mainWidget());
  dialog.setRange(range[0], range[1]);
  if (dialog.exec() == QDialog::Accepted) {
    vtkSMTransferFunctionProxy::RescaleTransferFunction(
      m_LUTProxy, dialog.minimum(), dialog.maximum());
  }
  renderViews();
  emit colorMapUpdated();
}

void HistogramWidget::onInvertClicked()
{
  vtkSMTransferFunctionProxy::InvertTransferFunction(m_LUTProxy);
  renderViews();
  emit colorMapUpdated();
}

void HistogramWidget::onPresetClicked()
{
  pqPresetDialog dialog(pqCoreUtilities::mainWidget(),
                        pqPresetDialog::SHOW_NON_INDEXED_COLORS_ONLY);
  dialog.setCustomizableLoadColors(true);
  dialog.setCustomizableLoadOpacities(true);
  dialog.setCustomizableUsePresetRange(true);
  dialog.setCustomizableLoadAnnotations(false);
  connect(&dialog, SIGNAL(applyPreset(const Json::Value&)),
          SLOT(applyCurrentPreset()));
  dialog.exec();
}

void HistogramWidget::applyCurrentPreset()
{
  auto dialog = qobject_cast<pqPresetDialog*>(sender());
  Q_ASSERT(dialog);

  vtkSMProxy* lut = m_LUTProxy;
  if (!lut) {
    return;
  }

  if (dialog->loadColors() || dialog->loadOpacities()) {
    vtkSMProxy* sof =
      vtkSMPropertyHelper(lut, "ScalarOpacityFunction", true).GetAsProxy();
    if (dialog->loadColors()) {
      vtkSMTransferFunctionProxy::ApplyPreset(lut, dialog->currentPreset(),
                                              !dialog->usePresetRange());
    }
    if (dialog->loadOpacities()) {
      if (sof) {
        vtkSMTransferFunctionProxy::ApplyPreset(sof, dialog->currentPreset(),
                                                !dialog->usePresetRange());
      } else {
        qWarning("Cannot load opacities since 'ScalarOpacityFunction' is not "
                 "present.");
      }
    }

    // We need to take extra care to avoid the color and opacity function ranges
    // from straying away from each other. This can happen if only one of them
    // is getting a preset and we're using the preset range.
    if (dialog->usePresetRange() &&
        (dialog->loadColors() ^ dialog->loadOpacities()) && sof) {
      double range[2];
      if (dialog->loadColors() &&
          vtkSMTransferFunctionProxy::GetRange(lut, range)) {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(sof, range);
      } else if (dialog->loadOpacities() &&
                 vtkSMTransferFunctionProxy::GetRange(sof, range)) {
        vtkSMTransferFunctionProxy::RescaleTransferFunction(lut, range);
      }
    }
    renderViews();
    emit colorMapUpdated();
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

void HistogramWidget::setGradientOpacityChecked(bool checked)
{
  m_gradientOpacityButton->setChecked(checked);
  emit m_gradientOpacityButton->toggled(checked);
}

void HistogramWidget::setGradientOpacityEnabled(bool enable)
{
  m_gradientOpacityButton->setEnabled(enable);
}
}

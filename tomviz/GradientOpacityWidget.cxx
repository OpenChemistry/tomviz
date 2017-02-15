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
#include "GradientOpacityWidget.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "ModuleContour.h"
#include "ModuleManager.h"
#include "Utilities.h"

#include "vtkChartGradientOpacityEditor.h"

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
#include <vtkSMViewProxy.h>

#include <QColorDialog>
#include <QHBoxLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <QDebug>

namespace tomviz {

GradientOpacityWidget::GradientOpacityWidget(QWidget* parent_)
  : QWidget(parent_), m_qvtk(new QVTKOpenGLWidget(this))
{
  // Set up our little chart.
  vtkNew<vtkGenericOpenGLRenderWindow> window_;
  m_qvtk->SetRenderWindow(window_.Get());
  m_histogramView->SetRenderWindow(window_.Get());
  m_histogramView->SetInteractor(m_qvtk->GetInteractor());
  m_histogramView->GetScene()->AddItem(m_histogramColorOpacityEditor.Get());

  // Connect events from the histogram color/opacity editor.
  m_eventLink->Connect(m_histogramColorOpacityEditor.Get(),
                       vtkCommand::EndEvent, this,
                       SLOT(onOpacityFunctionChanged()));

  auto hLayout = new QHBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  auto vLayout = new QVBoxLayout;
  hLayout->addLayout(vLayout);
  hLayout->setContentsMargins(0, 0, 5, 0);

  vLayout->setContentsMargins(0, 0, 0, 0);
  vLayout->addStretch(1);

//  auto button = new QToolButton;
//  button->setIcon(QIcon(":/pqWidgets/Icons/pqResetRange24.png"));
//  button->setToolTip("Reset data range");
//  connect(button, SIGNAL(clicked()), this, SLOT(onResetRangeClicked()));
//  vLayout->addWidget(button);
//
//  button = new QToolButton;
//  button->setIcon(QIcon(":/pqWidgets/Icons/pqResetRangeCustom24.png"));
//  button->setToolTip("Specify data range");
//  connect(button, SIGNAL(clicked()), this, SLOT(onCustomRangeClicked()));
//  vLayout->addWidget(button);
//
//  button = new QToolButton;
//  button->setIcon(QIcon(":/pqWidgets/Icons/pqInvert24.png"));
//  button->setToolTip("Invert color map");
//  connect(button, SIGNAL(clicked()), this, SLOT(onInvertClicked()));
//  vLayout->addWidget(button);
//
//  button = new QToolButton;
//  button->setIcon(QIcon(":/pqWidgets/Icons/pqFavorites16.png"));
//  button->setToolTip("Choose preset color map");
//  connect(button, SIGNAL(clicked()), this, SLOT(onPresetClicked()));
//  vLayout->addWidget(button);
//
//  vLayout->addStretch(1);

  setLayout(hLayout);
}

GradientOpacityWidget::~GradientOpacityWidget() = default;

void GradientOpacityWidget::setLUT(vtkPiecewiseFunction* transferFunc)
{
  if (m_scalarOpacityFunction) {
    m_eventLink->Disconnect(m_scalarOpacityFunction,
                            vtkCommand::ModifiedEvent, this,
                            SLOT(onOpacityFunctionChanged()));
  }

  m_scalarOpacityFunction = transferFunc;
  m_eventLink->Connect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent,
                         this, SLOT(onOpacityFunctionChanged()));
}

void GradientOpacityWidget::setInputData(vtkTable* table, const char* x_,
                                   const char* y_)
{
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  m_histogramView->Render();
}

void GradientOpacityWidget::onOpacityFunctionChanged()
{
  auto core = pqApplicationCore::instance();
  auto smModel = core->getServerManagerModel();
  QList<pqView*> views = smModel->findItems<pqView*>();
  foreach (pqView* view, views) {
    view->render();
  }

  // Update the histogram
  m_histogramView->GetRenderWindow()->Render();

  // Update the scalarOpacityFunc? TODO
}

void GradientOpacityWidget::onPresetClicked()
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

void GradientOpacityWidget::applyCurrentPreset()
{
  // The preset is supposed to be flat ones.
}

void GradientOpacityWidget::renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}
}

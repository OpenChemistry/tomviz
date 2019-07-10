/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "GradientOpacityWidget.h"

#include "ActiveObjects.h"
#include "QVTKGLWidget.h"
#include "Utilities.h"

#include <vtkChartGradientOpacityEditor.h>
#include <vtkContextScene.h>
#include <vtkContextView.h>
#include <vtkEventQtSlotConnect.h>
#include <vtkFloatArray.h>
#include <vtkIntArray.h>
#include <vtkPiecewiseFunction.h>
#include <vtkRenderWindow.h>
#include <vtkStdString.h>
#include <vtkTable.h>
#include <vtkVector.h>

#include <pqApplicationCore.h>
#include <pqView.h>
#include <vtkSMViewProxy.h>

#include <QHBoxLayout>
#include <QTimer>

namespace tomviz {

GradientOpacityWidget::GradientOpacityWidget(QWidget* parent_)
  : QWidget(parent_), m_qvtk(new QVTKGLWidget(this)), m_adjustedTable(nullptr)
{
  // Set up our little chart.
  m_histogramView->SetRenderWindow(m_qvtk->renderWindow());
  m_histogramView->SetInteractor(m_qvtk->interactor());
  m_histogramView->GetScene()->AddItem(m_histogramColorOpacityEditor);

  // Connect events from the histogram color/opacity editor.
  m_eventLink->Connect(m_histogramColorOpacityEditor,
                       vtkCommand::EndEvent, this,
                       SLOT(onOpacityFunctionChanged()));

  // Offset margins to align with HistogramWidget
  auto hLayout = new QHBoxLayout(this);
  hLayout->addWidget(m_qvtk);
  hLayout->setContentsMargins(0, 0, 35, 0);

  setLayout(hLayout);
}

GradientOpacityWidget::~GradientOpacityWidget() = default;

void GradientOpacityWidget::setLUT(vtkPiecewiseFunction* gradientOpac)
{
  if (m_scalarOpacityFunction) {
    m_eventLink->Disconnect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent,
                            this, SLOT(onOpacityFunctionChanged()));
  }

  m_scalarOpacityFunction = gradientOpac;
  if (!m_scalarOpacityFunction) {
    return;
  }

  m_eventLink->Connect(m_scalarOpacityFunction, vtkCommand::ModifiedEvent, this,
                       SLOT(onOpacityFunctionChanged()));
}

void GradientOpacityWidget::setInputData(vtkTable* table, const char* x_,
                                         const char* y_)
{
  if (table) {
    prepareAdjustedTable(table, x_);
  } else {
    m_adjustedTable = nullptr;
  }

  m_histogramColorOpacityEditor->SetHistogramInputData(m_adjustedTable, x_, y_);
  m_histogramColorOpacityEditor->SetOpacityFunction(m_scalarOpacityFunction);
  m_histogramView->Render();
}

void GradientOpacityWidget::prepareAdjustedTable(vtkTable* table,
                                                 const char* x_)
{
  m_adjustedTable = vtkSmartPointer<vtkTable>::New();

  vtkDataArray* array = vtkDataArray::SafeDownCast(table->GetColumnByName(x_));
  double* range = array->GetRange();
  const vtkIdType numBins = array->GetNumberOfTuples();

  vtkFloatArray* extents = vtkFloatArray::New();
  extents->SetName(vtkStdString("image_extents").c_str());
  extents->SetNumberOfComponents(1);
  extents->SetNumberOfTuples(numBins);
  const float step = static_cast<float>((range[1] - range[0]) /
                                        (4.0 * static_cast<double>(numBins)));

  auto extentsData = static_cast<float*>(extents->GetVoidPointer(0));
  for (vtkIdType i = 0; i < numBins; i++) {
    extentsData[i] = i * step;

    // Add two extra steps in the last table value. This is done to adjust the
    // range of the chart so that the last point is not occluded.
    if (i == numBins - 1) {
      extentsData[i] = (i + 2) * step;
    }
  }

  vtkIntArray* pops = vtkIntArray::New();
  pops->SetName(vtkStdString("image_pops").c_str());
  pops->SetNumberOfComponents(1);
  pops->SetNumberOfTuples(numBins);
  // Initialize with a value > 1.0 so that the y-axis range displays correctly
  memset(pops->GetVoidPointer(0), 10,
         sizeof(int) * static_cast<size_t>(numBins));

  m_adjustedTable->AddColumn(extents);
  m_adjustedTable->AddColumn(pops);
  extents->Delete();
  pops->Delete();
}

void GradientOpacityWidget::onOpacityFunctionChanged()
{
  auto core = pqApplicationCore::instance();
  auto smModel = core->getServerManagerModel();
  QList<pqView*> views = smModel->findItems<pqView*>();
  foreach (pqView* view, views) {
    view->render();
  }
  m_histogramView->GetRenderWindow()->Render();
}

void GradientOpacityWidget::renderViews()
{
  pqView* view =
    tomviz::convert<pqView*>(ActiveObjects::instance().activeView());
  if (view) {
    view->render();
  }
}
} // namespace tomviz

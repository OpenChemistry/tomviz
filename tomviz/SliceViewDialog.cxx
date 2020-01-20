/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SliceViewDialog.h"

#include <vtkImageProperty.h>
#include <vtkImageSlice.h>
#include <vtkImageSliceMapper.h>
#include <vtkInteractorStyleRubberBand2D.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkScalarsToColors.h>

#include <QDialog>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QRadioButton>
#include <QVBoxLayout>

#include "DataSource.h"
#include "QVTKGLWidget.h"
#include "Utilities.h"

#include <vtkImageData.h>

namespace tomviz {

SliceViewDialog::SliceViewDialog(QWidget* parent) : QDialog(parent)
{
  // Pick a reasonable size. It is very tiny otherwise.
  resize(500, 500);
  auto* vLayout = new QVBoxLayout(this);

  // Set the margins to all be 0
  vLayout->setContentsMargins(0, 0, 0, 0);

  m_glWidget = new QVTKGLWidget(this);
  vLayout->addWidget(m_glWidget);

  m_slice->SetMapper(m_mapper);
  m_renderer->AddViewProp(m_slice);

  m_glWidget->renderWindow()->AddRenderer(m_renderer);

  vtkNew<vtkInteractorStyleRubberBand2D> interatorStyle;
  interatorStyle->SetRenderOnMouseMove(true);
  m_glWidget->interactor()->SetInteractorStyle(interatorStyle);

  // Add in the radio buttons
  m_darkButton = new QRadioButton(this);
  m_whiteButton = new QRadioButton(this);

  auto* buttonLayout = new QHBoxLayout;
  vLayout->addLayout(buttonLayout);

  buttonLayout->addWidget(m_darkButton);
  buttonLayout->addWidget(m_whiteButton);

  m_darkButton->setText("Dark");
  m_whiteButton->setText("White");

  setupConnections();
}

void SliceViewDialog::setupConnections()
{
  connect(m_darkButton, &QRadioButton::clicked, this,
          &SliceViewDialog::switchToDark);
  connect(m_whiteButton, &QRadioButton::clicked, this,
          &SliceViewDialog::switchToWhite);
}

SliceViewDialog::~SliceViewDialog() = default;

void SliceViewDialog::setActiveImageData(vtkImageData* image)
{
  m_mapper->SetInputData(image);
  setSliceNumber(0);
  updateLUTRange();

  // Set up the renderer to show the slice in parallel projection
  // It also zooms the renderer so the entire slice is visible
  tomviz::setupRenderer(m_renderer, m_mapper);
  m_glWidget->renderWindow()->Render();
}

void SliceViewDialog::setSliceNumber(int slice)
{
  m_mapper->SetSliceNumber(slice);
  m_mapper->Update();
}

void SliceViewDialog::setLookupTable(vtkScalarsToColors* lut)
{
  m_slice->GetProperty()->SetLookupTable(lut);
}

void SliceViewDialog::updateLUTRange()
{
  // TODO: rescale the LUT for the current image data
}

void SliceViewDialog::switchToDark()
{
  m_darkButton->setChecked(true);
  m_whiteButton->setChecked(false);

  setActiveImageData(m_darkImage);
}

void SliceViewDialog::switchToWhite()
{
  m_whiteButton->setChecked(true);
  m_darkButton->setChecked(false);

  setActiveImageData(m_whiteImage);
}

} // namespace tomviz

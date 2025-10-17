/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "OperatorPropertiesPanel.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "EditOperatorDialog.h"
#include "Operator.h"
#include "OperatorWidget.h"
#include "Pipeline.h"
#include "Utilities.h"

#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace tomviz {

OperatorPropertiesPanel::OperatorPropertiesPanel(QWidget* p) : QWidget(p)
{
  // Show active module in the "Operator Properties" panel.
  connect(&ActiveObjects::instance(), SIGNAL(operatorActivated(Operator*)),
          SLOT(setOperator(Operator*)));

  // Set up a very simple layout with a description label widget.
  m_layout = new QVBoxLayout;
  setLayout(m_layout);
}

OperatorPropertiesPanel::~OperatorPropertiesPanel() = default;

void OperatorPropertiesPanel::setOperator(Operator* op)
{
  if (m_activeOperator) {
    disconnect(op, SIGNAL(labelModified()));
  }
  deleteLayoutContents(m_layout);
  m_operatorWidget = nullptr;
  if (op) {
    // See if we are dealing with a Python operator
    OperatorPython* pythonOperator = qobject_cast<OperatorPython*>(op);
    if (pythonOperator) {
      setOperator(pythonOperator);
    } else {
      auto description = new QLabel(op->label());
      layout()->addWidget(description);
      connect(op, &Operator::labelModified, m_activeOperator, [this, description]() {
        description->setText(m_activeOperator->label());
      });
    }

    m_layout->addStretch();
  }

  m_activeOperator = op;
}

void OperatorPropertiesPanel::setOperator(OperatorPython* op)
{
  auto buttonLayout = new QHBoxLayout;
  auto viewCodeButton = new QPushButton("View Code", this);

  connect(viewCodeButton, &QAbstractButton::clicked, this,
          &OperatorPropertiesPanel::viewCodePressed);
  buttonLayout->addWidget(viewCodeButton);

  m_operatorWidget = new OperatorWidget(this);
  m_operatorWidget->setupUI(op);

  // Check if we have any UI for this operator, there is probably a nicer
  // way todo this.
  if (!m_operatorWidget->layout() || m_operatorWidget->layout()->count() == 0) {
    m_operatorWidget->deleteLater();
    m_operatorWidget = nullptr;
  } else {
    // For now add to scroll box, our operator widget tend to be a little
    // wide!
    auto scroll = new QScrollArea(this);
    scroll->setWidget(m_operatorWidget);
    scroll->setWidgetResizable(true);

    m_layout->addWidget(scroll);

    auto apply =
      new QDialogButtonBox(QDialogButtonBox::Apply, Qt::Horizontal, this);
    connect(apply, &QDialogButtonBox::clicked, this,
            &OperatorPropertiesPanel::apply);

    buttonLayout->addWidget(apply);
  }
  m_layout->addItem(buttonLayout);
}

void OperatorPropertiesPanel::apply()
{
  if (m_operatorWidget) {
    auto values = m_operatorWidget->values();
    OperatorPython* pythonOperator =
      qobject_cast<OperatorPython*>(m_activeOperator);
    if (pythonOperator) {
      DataSource* dataSource =
        qobject_cast<DataSource*>(pythonOperator->parent());
      if (dataSource->pipeline()->isRunning()) {
        auto result = QMessageBox::question(
          this, "Cancel running operation?",
          "Applying changes to an operator that is part of a running pipeline "
          "will cancel the current running operator and restart the pipeline "
          "run.  Proceed anyway?");
        // FIXME There is still a concurrency issue here if the background
        // thread running the operator finishes and the finished event is queued
        // behind the question() return event above.  If that happens then we
        // will not get a canceled() event and the pipeline will stay paused.
        if (result == QMessageBox::No) {
          return;
        } else {
          auto whenCanceled = [pythonOperator, dataSource]() {
            // Resume the pipeline and emit transformModified
            dataSource->pipeline()->resume();
            emit pythonOperator->transformModified();
          };
          // We pause the pipeline so applyChangesToOperator does cause it to
          // execute.
          dataSource->pipeline()->pause();
          // We do this before causing cancel so the values are in place for
          // when
          // whenCanceled cause the pipeline to be re-executed.
          pythonOperator->setArguments(values);
          if (dataSource->pipeline()->isRunning()) {
            dataSource->pipeline()->cancel(whenCanceled);
          } else {
            whenCanceled();
          }
        }
      } else {
        pythonOperator->setArguments(values);
      }
    }
  }
}

void OperatorPropertiesPanel::viewCodePressed()
{
  EditOperatorDialog::showDialogForOperator(m_activeOperator,
                                            QStringLiteral("viewCode"));
}
} // namespace tomviz

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "SetTiltAnglesOperator.h"

#include "DataSource.h"
#include "EditOperatorWidget.h"

#include <vtkDataArray.h>
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkTypeInt8Array.h>

#include <QClipboard>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <cmath>

namespace {
class SetTiltAnglesWidget : public tomviz::EditOperatorWidget
{
  Q_OBJECT
public:
  SetTiltAnglesWidget(tomviz::SetTiltAnglesOperator* op,
                      vtkSmartPointer<vtkDataObject> dataObject, QWidget* p)
    : EditOperatorWidget(p), Op(op)
  {
    QMap<size_t, double> tiltAngles = this->Op->tiltAngles();
    QHBoxLayout* baseLayout = new QHBoxLayout;
    this->setLayout(baseLayout);
    this->tabWidget = new QTabWidget;
    baseLayout->addWidget(this->tabWidget);

    QWidget* setAutomaticPanel = new QWidget;

    QGridLayout* layout = new QGridLayout;

    QString descriptionString =
      "A tomographic \"tilt series\" is a set of projection images taken while "
      "rotating (\"tilting\") the specimen. Setting the correct angles is "
      "needed for accurate reconstruction. Set a linearly spaced range of "
      "angles by specifying the start and end tilt index and start and end "
      "angles.  The tilt angles can also be set in the \"Data Properties\" "
      "panel or from Python.";

    vtkImageData* image = vtkImageData::SafeDownCast(dataObject);
    Q_ASSERT(image);

    int extent[6];
    image->GetExtent(extent);
    int totalSlices = extent[5] - extent[4] + 1;
    this->previousTiltAngles.resize(totalSlices);
    if (totalSlices < 60) {
      angleIncrement = 3.0;
    } else if (totalSlices < 80) {
      angleIncrement = 2.0;
    } else if (totalSlices < 120) {
      angleIncrement = 1.5;
    } else {
      angleIncrement = 1.0;
    }

    double startAngleValue = -(totalSlices - 1) * angleIncrement / 2.0;
    double endAngleValue = startAngleValue + (totalSlices - 1) * angleIncrement;

    if (tiltAngles.contains(0) && tiltAngles.contains(totalSlices - 1)) {
      startAngleValue = tiltAngles[0];
      endAngleValue = tiltAngles[totalSlices - 1];
    }

    QLabel* descriptionLabel = new QLabel(descriptionString);
    descriptionLabel->setMinimumHeight(120);
    descriptionLabel->setSizePolicy(QSizePolicy::MinimumExpanding,
                                    QSizePolicy::MinimumExpanding);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel, 0, 0, 1, 4, Qt::AlignCenter);
    layout->addWidget(new QLabel("Start Image #: "), 1, 0, 1, 1,
                      Qt::AlignCenter);
    this->startTilt = new QSpinBox;
    startTilt->setRange(0, totalSlices - 1);
    startTilt->setValue(0);
    layout->addWidget(startTilt, 1, 1, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("End Image #: "), 2, 0, 1, 1, Qt::AlignCenter);
    this->endTilt = new QSpinBox;
    endTilt->setRange(0, totalSlices - 1);
    endTilt->setValue(totalSlices - 1);
    layout->addWidget(endTilt, 2, 1, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("Set Start Angle: "), 1, 2, 1, 1,
                      Qt::AlignCenter);
    this->startAngle = new QDoubleSpinBox;
    startAngle->setRange(-360.0, 360.0);
    startAngle->setValue(startAngleValue);
    layout->addWidget(startAngle, 1, 3, 1, 1, Qt::AlignCenter);
    layout->addWidget(new QLabel("Set End Angle: "), 2, 2, 1, 1,
                      Qt::AlignCenter);
    this->endAngle = new QDoubleSpinBox;
    endAngle->setRange(-360.0, 360.0);
    endAngle->setValue(endAngleValue);
    layout->addWidget(endAngle, 2, 3, 1, 1, Qt::AlignCenter);

    layout->addWidget(new QLabel("Angle Increment: "), 3, 2, 1, 1,
                      Qt::AlignCenter);

    QString s = QString::number(angleIncrement, 'f', 2);
    this->angleIncrementLabel = new QLabel(s);
    connect(startTilt, SIGNAL(valueChanged(int)), this,
            SLOT(updateAngleIncrement()));
    connect(endTilt, SIGNAL(valueChanged(int)), this,
            SLOT(updateAngleIncrement()));
    connect(startAngle, SIGNAL(valueChanged(double)), this,
            SLOT(updateAngleIncrement()));
    connect(endAngle, SIGNAL(valueChanged(double)), this,
            SLOT(updateAngleIncrement()));
    layout->addWidget(angleIncrementLabel, 3, 3, 1, 1, Qt::AlignCenter);

    auto outerLayout = new QVBoxLayout;
    outerLayout->addLayout(layout);
    outerLayout->addStretch();

    setAutomaticPanel->setLayout(outerLayout);

    QWidget* setFromTablePanel = new QWidget;
    QVBoxLayout* tablePanelLayout = new QVBoxLayout;
    this->tableWidget = new QTableWidget;
    this->tableWidget->setRowCount(totalSlices);
    this->tableWidget->setColumnCount(1);
    tablePanelLayout->addWidget(this->tableWidget);

    // Widget to hold tilt angle import button
    QWidget* buttonWidget = new QWidget;
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonWidget->setLayout(buttonLayout);
    tablePanelLayout->addWidget(buttonWidget);

    // Add button to load a text file with tilt series values
    QPushButton* loadFromFileButton = new QPushButton;
    loadFromFileButton->setText("Load From Text File");
    buttonLayout->addWidget(loadFromFileButton);
    buttonLayout->insertStretch(-1);
    connect(loadFromFileButton, SIGNAL(clicked()), SLOT(loadFromFile()));

    vtkFieldData* fd = dataObject->GetFieldData();
    vtkDataArray* tiltArray = nullptr;
    if (fd) {
      tiltArray = fd->GetArray("tilt_angles");
    }
    for (vtkIdType i = 0; i < totalSlices; ++i) {
      QTableWidgetItem* item = new QTableWidgetItem;
      double angle;
      // Make sure to set the previous angles
      if (tiltArray && i < tiltArray->GetNumberOfTuples()) {
        angle = tiltArray->GetTuple1(i);
        this->previousTiltAngles[i] = angle;
      } else {
        angle = 0;
        this->previousTiltAngles[i] = 0;
      }
      // deliberate non-else-if.  We want to override with the previous value
      // from the operator, but we need the previousTiltAngles array set
      // correctly, so the above condition has to be separate.
      if (tiltAngles.contains(i)) {
        angle = tiltAngles[i];
      }
      item->setData(Qt::DisplayRole, QString::number(angle));
      this->tableWidget->setItem(i, 0, item);
    }
    tableWidget->installEventFilter(this);

    setFromTablePanel->setLayout(tablePanelLayout);

    this->tabWidget->addTab(setAutomaticPanel, "Set by Range");
    this->tabWidget->addTab(setFromTablePanel, "Set Individually");

    baseLayout->setSizeConstraint(QLayout::SetMinimumSize);
  }

  void applyChangesToOperator() override
  {
    if (this->tabWidget->currentIndex() == 0) {
      QMap<size_t, double> tiltAngles = this->Op->tiltAngles();
      int start = startTilt->value();
      int end = endTilt->value();
      if (end == start) {
        tiltAngles[start] = startAngle->value();
      } else {
        double delta =
          (endAngle->value() - startAngle->value()) / (end - start);
        double baseAngle = startAngle->value();
        if (delta < 0) {
          int temp = start;
          start = end;
          end = temp;
          baseAngle = endAngle->value();
        }
        for (int i = 0; start + i <= end; ++i) {
          tiltAngles.insert(start + i, baseAngle + delta * i);
          this->tableWidget->item(i, 0)->setData(
            Qt::DisplayRole, QString::number(tiltAngles[start + i]));
        }
      }
      this->Op->setTiltAngles(tiltAngles);
    } else {
      QMap<size_t, double> tiltAngles;
      for (vtkIdType i = 0; i < this->tableWidget->rowCount(); ++i) {
        QTableWidgetItem* item = this->tableWidget->item(i, 0);
        tiltAngles[i] = item->data(Qt::DisplayRole).toDouble();
      }
      this->Op->setTiltAngles(tiltAngles);
    }
  }

  bool eventFilter(QObject* obj, QEvent* event) override
  {
    QKeyEvent* ke = dynamic_cast<QKeyEvent*>(event);
    if (ke && obj == this->tableWidget) {
      if (ke->matches(QKeySequence::Paste) && ke->type() == QEvent::KeyPress) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        const QMimeData* mimeData = clipboard->mimeData();
        if (mimeData->hasText()) {
          // some spreadsheet programs include an trailing newline when copying
          // a range of cells which then gets split into an empty string entry.
          // Strip this trailing newline.
          QString text = mimeData->text().trimmed();
          QStringList rows = text.split("\n");
          QStringList angles;
          for (const QString& row : rows) {
            angles << row.split("\t")[0];
          }
          auto ranges = this->tableWidget->selectedRanges();
          // check if the table in the clipboard is of numbers
          for (const QString& angle : angles) {
            bool ok;
            angle.toDouble(&ok);
            if (!ok) {
              QMessageBox::warning(
                this, "Error",
                QString("Error: pasted tilt angle %1 is not a number")
                  .arg(angle));
              return true;
            }
          }
          // If separate blocks of rows selected, cancel the paste
          // since we don't know where to put angles
          if (ranges.size() != 1) {
            QMessageBox::warning(
              this, "Error",
              "Pasting is not supported with non-continuous selections");
            return true;
          }
          // If multiple rows selected and it is not equal to
          // the number of angles pasted, cancel the paste
          if (ranges[0].rowCount() > 1 &&
              ranges[0].rowCount() != angles.size()) {
            QMessageBox::warning(this, "Error",
                                 QString("Cells selected (%1) does not match "
                                         "number of angles to paste (%2).  \n"
                                         "Please select one cell to mark the "
                                         "start location for pasting or select "
                                         "the same number of cells that will "
                                         "be pasted into.")
                                   .arg(ranges[0].rowCount())
                                   .arg(angles.size()));
            return true;
          }
          int startRow = ranges[0].topRow();
          for (int i = 0; i < angles.size(); ++i) {
            auto item = this->tableWidget->item(i + startRow, 0);
            if (item) {
              item->setData(Qt::DisplayRole, angles[i]);
            }
          }
        }
        return true;
      }
    }
    return QWidget::eventFilter(obj, event);
  }

public slots:
  void updateAngleIncrement()
  {
    angleIncrement = (endAngle->value() - startAngle->value()) /
                     (endTilt->value() - startTilt->value());
    QString s;
    if (std::isfinite(angleIncrement)) {
      s = QString::number(angleIncrement, 'f', 2);
    } else if (endAngle->value() == startAngle->value()) {
      s = QString::number(0, 'f', 2);
    } else {
      s = "Invalid inputs!";
    }
    this->angleIncrementLabel->setText(s);
  }

  void loadFromFile()
  {
    QStringList filters;
    filters << "Any (*)"
            << "Text (*.txt)"
            << "CSV (*.csv)";

    QFileDialog dialog;
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilters(filters);
    dialog.setObjectName("SetTiltAnglesOperator-loadFromFile");
    dialog.setAcceptMode(QFileDialog::AcceptOpen);

    if (dialog.exec() == QDialog::Accepted) {
      QString content;
      QFile tiltAnglesFile(dialog.selectedFiles()[0]);
      if (tiltAnglesFile.open(QIODevice::ReadOnly)) {
        content = tiltAnglesFile.readAll();
      } else {
        qCritical()
          << QString("Unable to read '%1'.").arg(dialog.selectedFiles()[0]);
      }

      QStringList angleStrings = content.split(QRegExp("\\s+"));
      int maxRows =
        std::min(angleStrings.size(), this->tableWidget->rowCount());
      for (int i = 0; i < maxRows; ++i) {
        QTableWidgetItem* item = this->tableWidget->item(i, 0);
        item->setData(Qt::DisplayRole, angleStrings[i]);
      }
    }
  }

private:
  QSpinBox* startTilt;
  QSpinBox* endTilt;
  QDoubleSpinBox* startAngle;
  QDoubleSpinBox* endAngle;
  QTableWidget* tableWidget;
  QTabWidget* tabWidget;
  QLabel* angleIncrementLabel;
  double angleIncrement = 1.0;

  QPointer<tomviz::SetTiltAnglesOperator> Op;
  QVector<double> previousTiltAngles;
};

#include "SetTiltAnglesOperator.moc"
} // namespace

namespace tomviz {

SetTiltAnglesOperator::SetTiltAnglesOperator(QObject* p) : Operator(p) {}

QIcon SetTiltAnglesOperator::icon() const
{
  return QIcon();
}

Operator* SetTiltAnglesOperator::clone() const
{
  SetTiltAnglesOperator* op = new SetTiltAnglesOperator;
  op->setTiltAngles(m_tiltAngles);
  return op;
}

QJsonObject SetTiltAnglesOperator::serialize() const
{
  auto json = Operator::serialize();

  // Note that this is always a dense array of angles, storing it as such.
  QJsonArray angleArray;
  for (auto itr = std::begin(m_tiltAngles); itr != std::end(m_tiltAngles);
       ++itr) {
    angleArray << itr.value();
  }

  json["angles"] = angleArray;
  return json;
}

bool SetTiltAnglesOperator::deserialize(const QJsonObject& json)
{
  if (json.contains("angles") && json["angles"].isArray()) {
    auto angleArray = json["angles"].toArray();
    m_tiltAngles.clear();
    for (int i = 0; i < angleArray.size(); ++i) {
      m_tiltAngles.insert(i, angleArray[i].toDouble());
    }
  }

  return true;
}

EditOperatorWidget* SetTiltAnglesOperator::getEditorContentsWithData(
  QWidget* p, vtkSmartPointer<vtkImageData> dataObject)
{
  return new SetTiltAnglesWidget(this, dataObject, p);
}

void SetTiltAnglesOperator::setTiltAngles(const QMap<size_t, double>& newAngles)
{
  m_tiltAngles = newAngles;
  emit transformModified();
}

bool SetTiltAnglesOperator::applyTransform(vtkDataObject* dataObject)
{
  vtkImageData* image = vtkImageData::SafeDownCast(dataObject);
  if (!image) {
    return false;
  }
  int extent[6];
  image->GetExtent(extent);
  int totalSlices = extent[5] - extent[4] + 1;
  vtkFieldData* fd = dataObject->GetFieldData();
  // Make sure the data is marked as a tilt series
  vtkTypeInt8Array* dataType =
    vtkTypeInt8Array::SafeDownCast(fd->GetArray("tomviz_data_source_type"));
  if (!dataType) {
    vtkNew<vtkTypeInt8Array> array;
    array->SetNumberOfTuples(1);
    array->SetName("tomviz_data_source_type");
    fd->AddArray(array.Get());
    dataType = array.Get();
  }
  // It should already be this value...
  dataType->SetTuple1(0, DataSource::TiltSeries);
  // Set the tilt angles
  vtkDataArray* dataTiltAngles = fd->GetArray("tilt_angles");
  if (!dataTiltAngles) {
    vtkNew<vtkDoubleArray> angles;
    angles->SetNumberOfTuples(totalSlices);
    angles->FillComponent(0, 0.0);
    angles->SetName("tilt_angles");
    fd->AddArray(angles.Get());
    dataTiltAngles = angles.Get();
  } else if (dataTiltAngles->GetNumberOfTuples() < totalSlices) {
    dataTiltAngles->SetNumberOfTuples(totalSlices);
  }
  for (auto itr = std::begin(m_tiltAngles); itr != std::end(m_tiltAngles);
       ++itr) {
    dataTiltAngles->SetTuple(itr.key(), &itr.value());
  }
  return true;
}
} // namespace tomviz

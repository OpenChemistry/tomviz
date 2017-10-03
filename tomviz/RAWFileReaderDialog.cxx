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
#include "RAWFileReaderDialog.h"
#include "ui_RAWFileReaderDialog.h"

#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"

#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QSpinBox>

namespace tomviz {

RAWFileReaderDialog::RAWFileReaderDialog(vtkSMProxy* reader, QWidget* p)
  : QDialog(p), m_ui(new Ui::RAWFileReaderDialog), m_reader(reader)
{
  // This will break when we support reading in raw data broken across multiple
  // files...
  vtkSMPropertyHelper fname(reader, "FilePrefix");
  QFileInfo info(fname.GetAsString());
  m_filesize = info.size();
  setWindowTitle(QString("Opening %1").arg(info.fileName()));
  m_ui->setupUi(this);
  connect(m_ui->dimensionX,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &RAWFileReaderDialog::sanityCheckSize);
  connect(m_ui->dimensionY,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &RAWFileReaderDialog::sanityCheckSize);
  connect(m_ui->dimensionZ,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &RAWFileReaderDialog::sanityCheckSize);
  connect(m_ui->dataType, static_cast<void (QComboBox::*)(int)>(
                            &QComboBox::currentIndexChanged),
          this, &RAWFileReaderDialog::sanityCheckSize);
  connect(m_ui->dataType, static_cast<void (QComboBox::*)(int)>(
                            &QComboBox::currentIndexChanged),
          this, &RAWFileReaderDialog::dataTypeChanged);
  connect(m_ui->signedness, &QCheckBox::stateChanged, this, &RAWFileReaderDialog::sanityCheckSize);
  connect(m_ui->numComponents,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &RAWFileReaderDialog::sanityCheckSize);
  connect(this, &QDialog::accepted, this, &RAWFileReaderDialog::onAccepted);
  sanityCheckSize();
}

RAWFileReaderDialog::~RAWFileReaderDialog()
{
}

void RAWFileReaderDialog::dimensions(size_t* output)
{
  output[0] = m_ui->dimensionX->value();
  output[1] = m_ui->dimensionY->value();
  output[2] = m_ui->dimensionZ->value();
}

int RAWFileReaderDialog::components()
{
  return m_ui->numComponents->value();
}

int RAWFileReaderDialog::vtkDataType()
{
  int idx = m_ui->dataType->currentIndex();
  bool sign = m_ui->signedness->isChecked();
  if (sign) {
    if (idx == 0) {
      return VTK_SIGNED_CHAR;
    } else if (idx == 1) {
      return VTK_SHORT;
    } else if (idx == 2) {
      return VTK_INT;
    } else if (idx == 3) {
#if VTK_SIZE_OF_LONG == 8
      return VTK_LONG;
#else
      // Windows
      return VTK_LONG_LONG;
#endif
    } else if (idx == 4) {
      return VTK_FLOAT;
    } else if (idx == 5) {
      return VTK_DOUBLE;
    }
  } else {
    if (idx == 0) {
      return VTK_UNSIGNED_CHAR;
    } else if (idx == 1) {
      return VTK_UNSIGNED_SHORT;
    } else if (idx == 2) {
      return VTK_UNSIGNED_INT;
    } else if (idx == 3) {
#if VTK_SIZE_OF_LONG == 8
      return VTK_UNSIGNED_LONG;
#else
      // Windows
      return VTK_UNSIGNED_LONG_LONG;
#endif
    } else if (idx == 4) {
      // This shouldn't happen...
      return VTK_FLOAT;
    } else if (idx == 5) {
      // This shouldn't happen...
      return VTK_DOUBLE;
    }
  }
  return VTK_CHAR;
}

void RAWFileReaderDialog::sanityCheckSize()
{
  int dataType = this->vtkDataType();
  size_t dims[3];
  this->dimensions(dims);
  size_t numComponents = m_ui->numComponents->value();
  size_t dataSize = 0;
  switch (dataType) {
    case VTK_SIGNED_CHAR:
    case VTK_UNSIGNED_CHAR:
      dataSize = sizeof(char);
      break;
    case VTK_SHORT:
    case VTK_UNSIGNED_SHORT:
      dataSize = sizeof(short);
      break;
    case VTK_INT:
    case VTK_UNSIGNED_INT:
      dataSize = sizeof(int);
      break;
    case VTK_LONG:
    case VTK_UNSIGNED_LONG:
      dataSize = sizeof(long);
      break;
    case VTK_LONG_LONG:
    case VTK_UNSIGNED_LONG_LONG:
      dataSize = sizeof(long long);
      break;
    case VTK_FLOAT:
      dataSize = sizeof(float);
      break;
    case VTK_DOUBLE:
      dataSize = sizeof(double);
      break;
  }
  size_t selectedSize =
    dataSize * dims[0] * dims[1] * dims[2] * numComponents;
  auto labelText =
    QString("Selected parameters would read %1 of %3 bytes (%2% of the file)")
      .arg(selectedSize)
      .arg(static_cast<float>(selectedSize) / m_filesize * 100)
      .arg(m_filesize);
  m_ui->statusLabel->setText(labelText);
  if (selectedSize != m_filesize) {
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
  } else {
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
  }
}

void RAWFileReaderDialog::dataTypeChanged()
{
  int typeIndex = m_ui->dataType->currentIndex();
  if (typeIndex < 4) {
    m_ui->signedness->setEnabled(true);
  } else {
    m_ui->signedness->setEnabled(false);
    m_ui->signedness->setChecked(true);
  }
}

void RAWFileReaderDialog::onAccepted()
{
  vtkSMPropertyHelper scalarType(m_reader, "DataScalarType");
  vtkSMPropertyHelper byteOrder(m_reader, "DataByteOrder");
  vtkSMPropertyHelper numComps(m_reader, "NumberOfScalarComponents");
  vtkSMPropertyHelper dims(m_reader, "DataExtent");

  scalarType.Set(this->vtkDataType());
  byteOrder.Set(m_ui->endianness->currentIndex());
  numComps.Set(m_ui->numComponents->value());

  int dimensionX = m_ui->dimensionX->value();
  int dimensionY = m_ui->dimensionY->value();
  int dimensionZ = m_ui->dimensionZ->value();
  int extents[6];
  extents[0] = extents[2] = extents[4] = 0;
  extents[1] = dimensionX - 1;
  extents[3] = dimensionY - 1;
  extents[5] = dimensionZ - 1;

  dims.Set(extents, 6);
  m_reader->UpdateVTKObjects();
}
}

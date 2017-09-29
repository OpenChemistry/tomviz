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
#include <QPushButton>
#include <QSpinBox>
#include <QtGlobal>

namespace tomviz {

RAWFileReaderDialog::RAWFileReaderDialog(vtkSMProxy* reader, size_t filesize,
                                         QWidget* p)
  : QDialog(p), m_ui(new Ui::RAWFileReaderDialog), m_reader(reader),
    m_filesize(filesize)
{
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
  connect(m_ui->numComponents,
          static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
          &RAWFileReaderDialog::sanityCheckSize);
  connect(this, &QDialog::accepted, this, &RAWFileReaderDialog::onAccepted);
  sanityCheckSize();
}

RAWFileReaderDialog::~RAWFileReaderDialog()
{
}

void RAWFileReaderDialog::dimensions(double* output)
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
  // The two is a conversion factor between the index in the dropdown and the
  // VTK type numbering
  // used by the RAW reader
  return m_ui->dataType->currentIndex() + 2;
}

void RAWFileReaderDialog::sanityCheckSize()
{
  int dataType = m_ui->dataType->currentIndex();
  size_t dimensionX = m_ui->dimensionX->value();
  size_t dimensionY = m_ui->dimensionY->value();
  size_t dimensionZ = m_ui->dimensionZ->value();
  size_t numComponents = m_ui->numComponents->value();
  size_t dataSize = 0;
  switch (dataType) {
    case 0: // char
      dataSize = sizeof(char);
      break;
    case 1: // unsigned char
      dataSize = sizeof(unsigned char);
      break;
    case 2: // short
      dataSize = sizeof(short);
      break;
    case 3: // unsigned short
      dataSize = sizeof(unsigned short);
      break;
    case 4: // int
      dataSize = sizeof(int);
      break;
    case 5: // unsigned int
      dataSize = sizeof(unsigned int);
      break;
    case 6: // long
      dataSize = sizeof(long);
      break;
    case 7: // unsigned long
      dataSize = sizeof(unsigned long);
      break;
    case 8: // float
      dataSize = sizeof(float);
      break;
    case 9: // double
      dataSize = sizeof(double);
      break;
  }
  size_t selectedSize =
    dataSize * dimensionX * dimensionY * dimensionZ * numComponents;
  if (selectedSize != m_filesize) {
    m_ui->statusLabel->setText(
      "The selected dimensions and data type do not match the file size!");
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
  } else {
    m_ui->statusLabel->setText("");
    m_ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
  }
}

void RAWFileReaderDialog::onAccepted()
{
  vtkSMPropertyHelper scalarType(m_reader, "DataScalarType");
  vtkSMPropertyHelper byteOrder(m_reader, "DataByteOrder");
  vtkSMPropertyHelper numComps(m_reader, "NumberOfScalarComponents");
  vtkSMPropertyHelper dims(m_reader, "DataExtent");

  scalarType.Set(m_ui->dataType->currentIndex() + 2);
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

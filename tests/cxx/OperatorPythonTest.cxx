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
#include <gtest/gtest.h>

#include <vtkDataObject.h>

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QIODevice>
#include <QString>

#include "OperatorPython.h"
#include "TomvizTest.h"

using namespace tomviz;

class OperatorPythonTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    dataObject = vtkDataObject::New();
    pythonOperator = new OperatorPython();
  }

  void TearDown() override
  {
    dataObject->Delete();
    pythonOperator->deleteLater();
  }

  vtkDataObject* dataObject;
  OperatorPython* pythonOperator;
};

TEST_F(OperatorPythonTest, transform_function)
{
  pythonOperator->setLabel("transform_function");
  QFile file(QString("%1/fixtures/function.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    pythonOperator->setScript(script);
    pythonOperator->transform(dataObject);
    file.close();
  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, operator_transform)
{
  pythonOperator->setLabel("operator_transform");
  QFile file(QString("%1/fixtures/operator.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    pythonOperator->setScript(script);
    ASSERT_TRUE(pythonOperator->transform(dataObject));
    file.close();
  } else {
    FAIL() << "Unable to load script.";
  }
}

TEST_F(OperatorPythonTest, cancelable_operator_transform)
{
  pythonOperator->setLabel("cancelable_operator_transform");
  QFile file(QString("%1/fixtures/cancelable.py").arg(SOURCE_DIR));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray array = file.readAll();
    QString script(array);
    file.close();
    pythonOperator->setScript(script);
    ASSERT_TRUE(pythonOperator->transform(dataObject));

    pythonOperator->cancelTransform();
    ASSERT_FALSE(pythonOperator->transform(dataObject));

  } else {
    FAIL() << "Unable to load script.";
  }
}

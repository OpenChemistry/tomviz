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
#ifndef tomvizTranslateAlignOperator_h
#define tomvizTranslateAlignOperator_h

#include "Operator.h"

#include "vtkVector.h"

#include <QPointer>
#include <QVector>

namespace tomviz {

class DataSource;

class TranslateAlignOperator : public Operator
{
  Q_OBJECT

public:
  TranslateAlignOperator(DataSource* dataSource, QObject* parent = nullptr);

  QString label() const override { return "Translation Align"; }
  QIcon icon() const override;
  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent, vtkSmartPointer<vtkImageData> data) override;

  void setAlignOffsets(const QVector<vtkVector2i>& offsets);
  const QVector<vtkVector2i>& getAlignOffsets() const { return offsets; }

  DataSource* getDataSource() const { return this->dataSource; }

  bool hasCustomUI() const override { return true; }

protected:
  bool applyTransform(vtkDataObject* data) override;

private:
  QVector<vtkVector2i> offsets;
  const QPointer<DataSource> dataSource;
};
} // namespace tomviz

#endif

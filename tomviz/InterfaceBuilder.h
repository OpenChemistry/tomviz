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
#ifndef tomvizInterfaceBuilder_h
#define tomvizInterfaceBuilder_h

#include <QGridLayout>
#include <QMap>
#include <QObject>
#include <QVariant>
#include <QJsonDocument>
#include <QJsonArray>

namespace tomviz {

class DataSource;

/// The InterfaceBuilder creates a Qt widget containing controls defined
/// by a JSON description.
class InterfaceBuilder : public QObject
{
  Q_OBJECT

public:
  InterfaceBuilder(QObject* parent = nullptr, DataSource* ds = nullptr);

  /// Set the JSON description
  void setJSONDescription(const QString& description);
  void setJSONDescription(const QJsonDocument& description);

  /// Build the interface, returning it in a QWidget.
  QLayout* buildInterface() const;
  QLayout* buildParameterInterface(QGridLayout* layout, QJsonArray &parameters) const;


  /// Set the parameter values
  void setParameterValues(QMap<QString, QVariant> values);

  static QMap<QString, QVariant> values(const QObject* parent);
private:
  Q_DISABLE_COPY(InterfaceBuilder)

  QJsonDocument m_json;
  QMap<QString, QVariant> m_parameterValues;
  DataSource* m_dataSource;
};

} // namespace tomviz

#endif

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

#include <QLayout>
#include <QMap>
#include <QObject>
#include <QVariant>

namespace tomviz {

/// The InterfaceBuilder creates a Qt widget containing controls defined
/// by a JSON description.
class InterfaceBuilder : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  InterfaceBuilder(QObject* parent = nullptr);
  ~InterfaceBuilder() override;

  /// Set the JSON description
  void setJSONDescription(const QString& description);

  /// Get the JSON description
  const QString& JSONDescription() const;

  /// Build the interface, returning it in a QWidget.
  QLayout* buildInterface() const;

  /// Set the parameter values
  void setParameterValues(QMap<QString, QVariant> values);

private:
  Q_DISABLE_COPY(InterfaceBuilder)

  QString m_json;
  QMap<QString, QVariant> m_parameterValues;
};

} // namespace tomviz

#endif

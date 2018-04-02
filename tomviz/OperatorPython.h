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
#ifndef tomvizOperatorPython_h
#define tomvizOperatorPython_h

#include "Operator.h"
#include <QMap>
#include <QMetaType>
#include <QScopedPointer>
#include <pqSMProxy.h>
#include <vtkDataObject.h>

namespace tomviz {

class CustomPythonOperatorWidget;

class OperatorPython : public Operator
{
  Q_OBJECT

public:
  OperatorPython(QObject* parent = nullptr);
  ~OperatorPython() override;

  QString label() const override { return m_label; }
  void setLabel(const QString& txt);

  /// Returns an icon to use for this operator.
  QIcon icon() const override;

  /// Return a new clone.
  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  bool serialize(pugi::xml_node& in) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  void setJSONDescription(const QString& str);
  const QString& JSONDescription() const;

  void setScript(const QString& str);
  const QString& script() const { return m_script; }

  EditOperatorWidget* getEditorContents(QWidget* parent) override;
  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent,
    vtkSmartPointer<vtkImageData> inputDataForDisplay) override;
  bool hasCustomUI() const override { return true; }

  /// Set the arguments to pass to the transform_scalars function
  void setArguments(QMap<QString, QVariant> args);

  /// Returns the argument that will be passed to transform_scalars
  QMap<QString, QVariant> arguments() const;

  typedef CustomPythonOperatorWidget* (*CustomWidgetFunction)(
    QWidget*, Operator*, vtkSmartPointer<vtkImageData>);

  static void registerCustomWidget(const QString& key, bool needsData,
                                   CustomWidgetFunction func);

signals:
  // Signal used to request the creation of a new data source. Needed to
  // ensure the initialization of the new DataSource is performed on UI thread
  void newChildDataSource(const QString&, vtkSmartPointer<vtkDataObject>);
  void newOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>);
  /// Signal uses to request that the child data source be updated with
  /// a new vtkDataObject.
  void childDataSourceUpdated(vtkSmartPointer<vtkDataObject>);

protected:
  bool applyTransform(vtkDataObject* data) override;

private slots:
  // Create a new child datasource and set it on this operator
  void createNewChildDataSource(const QString& label,
                                vtkSmartPointer<vtkDataObject>);
  void updateChildDataSource(vtkSmartPointer<vtkDataObject>);
  void setOperatorResult(const QString& name,
                         vtkSmartPointer<vtkDataObject> result);

private:
  Q_DISABLE_COPY(OperatorPython)

  class OPInternals;
  const QScopedPointer<OPInternals> d;
  QString m_label;
  QString m_jsonDescription;
  QString m_script;

  QString m_customWidgetID;

  QList<QString> m_resultNames;
  QList<QPair<QString, QString>> m_childDataSourceNamesAndLabels;
  QMap<QString, QVariant> m_arguments;
};
}
#endif

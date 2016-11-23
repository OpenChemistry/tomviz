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
#include <vtkSmartPointer.h>

Q_DECLARE_METATYPE(vtkSmartPointer<vtkDataObject>)

namespace tomviz {
class OperatorPython : public Operator
{
  Q_OBJECT
  typedef Operator Superclass;

public:
  OperatorPython(QObject* parent = nullptr);
  virtual ~OperatorPython();

  QString label() const override { return this->Label; }
  void setLabel(const QString& txt);

  /// Returns an icon to use for this operator.
  QIcon icon() const override;

  /// Return a new clone.
  Operator* clone() const override;

  bool serialize(pugi::xml_node& in) const override;
  bool deserialize(const pugi::xml_node& ns) override;

  void setJSONDescription(const QString& str);
  const QString& JSONDescription() const;

  void setScript(const QString& str);
  const QString& script() const { return this->Script; }

  EditOperatorWidget* getEditorContents(QWidget* parent) override;
  bool hasCustomUI() const override { return true; }

  /// Set the arguments to pass to the transform_scalars function
  void setArguments(QMap<QString, QVariant> args);

  /// Returns the argument that will be passed to transform_scalars
  QMap<QString, QVariant> arguments() const;

signals:
  // Signal used to request the creation of a new data source. Needed to
  // ensure the initialization of the new DataSource is performed on UI thread
  void newChildDataSource(const QString&, vtkSmartPointer<vtkDataObject>);
  void newOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>);

protected:
  bool applyTransform(vtkDataObject* data) override;

private slots:
  // Create a new child datasource and set it on this operator
  void createNewChildDataSource(const QString& label,
                                vtkSmartPointer<vtkDataObject>);
  void setOperatorResult(const QString& name,
                         vtkSmartPointer<vtkDataObject> result);

private:
  Q_DISABLE_COPY(OperatorPython)

  class OPInternals;
  const QScopedPointer<OPInternals> Internals;
  QString Label;
  QString jsonDescription;
  QString Script;

  QList<QString> m_resultNames;
  QList<QPair<QString, QString>> m_childDataSourceNamesAndLabels;
  QMap<QString, QVariant> m_arguments;
};
}
#endif

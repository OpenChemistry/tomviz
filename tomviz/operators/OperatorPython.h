/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorPython_h
#define tomvizOperatorPython_h

#include "Operator.h"
#include "PythonUtilities.h"
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
  // The parent must be a DataSource
  OperatorPython(DataSource* parent);
  ~OperatorPython() override;

  QString label() const override { return m_label; }
  void setLabel(const QString& txt);

  /// Returns an icon to use for this operator.
  QIcon icon() const override;

  /// Return a new clone.
  Operator* clone() const override;

  QJsonObject serialize() const override;
  bool deserialize(const QJsonObject& json) override;

  void setJSONDescription(const QString& str);
  const QString& JSONDescription() const;

  void setScript(const QString& str);
  const QString& script() const { return m_script; }

  bool preferCOrdering() const { return m_preferCOrdering; }

  EditOperatorWidget* getEditorContents(QWidget* parent) override;
  EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent,
    vtkSmartPointer<vtkImageData> inputDataForDisplay) override;
  bool hasCustomUI() const override { return true; }

  /// Set the arguments to pass to the transform_scalars function
  void setArguments(QMap<QString, QVariant> args);

  /// Returns the argument that will be passed to transform_scalars
  QMap<QString, QVariant> arguments() const;

  /// Not really "public" but needs to called when running pipeline externally.
  /// Needed to create the data source upfront for live updates.
  void createChildDataSource();

  /// Not really "public" but needs to called when running pipeline externally.
  bool updateChildDataSource(Python::Dict output);
  bool updateChildDataSource(
    QMap<QString, vtkSmartPointer<vtkDataObject>> output);

  typedef CustomPythonOperatorWidget* (*CustomWidgetFunction)(
    QWidget*, Operator*, vtkSmartPointer<vtkImageData>);

  static void registerCustomWidget(const QString& key, bool needsData,
                                   CustomWidgetFunction func);

  void setTypeInfo(const QMap<QString, QString>& typeInfo);
  const QMap<QString, QString>& typeInfo() const;

  int numberOfParameters() const { return m_numberOfParameters; }

  void setChildDataSource(DataSource* source) override;

signals:
  void newOperatorResult(const QString&, vtkSmartPointer<vtkDataObject>);
  /// Signal uses to request that the child data source be updated with
  /// a new vtkDataObject.
  void childDataSourceUpdated(vtkSmartPointer<vtkDataObject>);

protected:
  bool applyTransform(vtkDataObject* data) override;

private slots:
  void updateChildDataSource(vtkSmartPointer<vtkDataObject>);
  void setOperatorResult(const QString& name,
                         vtkSmartPointer<vtkDataObject> result);

private:
  Q_DISABLE_COPY(OperatorPython)

  void setNumberOfParameters(int n) { m_numberOfParameters = n; }
  void setHelpFromJson(const QJsonObject& json);
  class OPInternals;
  const QScopedPointer<OPInternals> d;
  QString m_label;
  QString m_jsonDescription;
  QString m_script;

  // This is for operators without a JSON description but with arguments.
  // Serialization needs to know the type of the arguments.
  QMap<QString, QString> m_typeInfo;

  QString m_customWidgetID;

  QList<QString> m_resultNames;
  QString m_childDataSourceName = "output";
  QString m_childDataSourceLabel = "Output";
  bool m_preferCOrdering = false;

  QMap<QString, QVariant> m_arguments;
  int m_numberOfParameters = 0;
};
} // namespace tomviz
#endif

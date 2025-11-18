/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizInterfaceBuilder_h
#define tomvizInterfaceBuilder_h

#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QObject>
#include <QVariant>

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
  QLayout* buildParameterInterface(QGridLayout* layout, QJsonArray& parameters,
                                   const QString& tag = "") const;

  /// Set the parameter values
  void setParameterValues(QMap<QString, QVariant> values);

  static QMap<QString, QVariant> parameterValues(const QObject* parent);

  void updateWidgetValues(const QObject* parent);

  void setupEnableAndVisibleStates(const QObject* parent,
                                   QJsonArray& parameters) const;
  void setupEnableStates(const QObject* parent, QJsonArray& parameters,
                         bool visible) const;

  QWidget* findWidgetByName(const QObject* parent, const QString& name) const;

private:
  Q_DISABLE_COPY(InterfaceBuilder)

  QJsonDocument m_json;
  QMap<QString, QVariant> m_parameterValues;
  DataSource* m_dataSource;
};

} // namespace tomviz

#endif

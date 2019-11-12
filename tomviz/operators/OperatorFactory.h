/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorFactory_h
#define tomvizOperatorFactory_h

#include <QObject>

#include "DataSource.h"

namespace tomviz {
class Operator;

struct PythonOperatorInfo
{
  QString label;
  QString source;
  bool requiresTiltSeries;
  bool requiresVolume;
  bool requiresFib;
  QString json;
};

class OperatorFactory
{
  typedef QObject Superclass;

public:
  static OperatorFactory& instance();

  /// Returns a list of module types
  QList<QString> operatorTypes();

  Operator* createConvertToVolumeOperator(
    DataSource::DataSourceType t = DataSource::Volume);

  /// Creates an operator of the given type
  Operator* createOperator(const QString& type, DataSource* ds);

  /// Returns the type for an operator instance.
  const char* operatorType(const Operator* module);

  /// Register a Python operator
  void registerPythonOperator(const QString& label, const QString& source,
                              bool requiresTiltSeries, bool requiresVolume,
                              bool requiresFib, const QString& json);

  /// Returns the list of register Python operators
  const QList<PythonOperatorInfo>& registeredPythonOperators();

private:
  OperatorFactory();
  ~OperatorFactory();
  Q_DISABLE_COPY(OperatorFactory)

  QList<PythonOperatorInfo> m_pythonOperators;
};
} // namespace tomviz

#endif

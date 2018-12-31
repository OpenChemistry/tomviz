/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorFactory_h
#define tomvizOperatorFactory_h

#include <QObject>

#include "DataSource.h"

namespace tomviz {
class Operator;

class OperatorFactory
{
  typedef QObject Superclass;

public:
  /// Returns a list of module types
  static QList<QString> operatorTypes();

  static Operator* createConvertToVolumeOperator(
    DataSource::DataSourceType t = DataSource::Volume);

  /// Creates an operator of the given type
  static Operator* createOperator(const QString& type, DataSource* ds);

  /// Returns the type for an operator instance.
  static const char* operatorType(const Operator* module);

private:
  OperatorFactory();
  ~OperatorFactory();
  Q_DISABLE_COPY(OperatorFactory)
};
} // namespace tomviz

#endif

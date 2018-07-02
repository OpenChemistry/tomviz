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

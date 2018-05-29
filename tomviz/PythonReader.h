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
#ifndef tomvizPythonReader_h
#define tomvizPythonReader_h

#include "PythonUtilities.h"

#include <QString>
#include <QStringList>

namespace tomviz{

class DataSource;

class PythonReader {
  public:
    PythonReader(QString, QStringList, Python::Function);
    DataSource* read(QString fileName);
    QString getDescription() const;
    QStringList getExtensions() const;

  private:
    QString m_description;
    QStringList m_extensions;
    Python::Function m_readFunction;
};
}

#endif

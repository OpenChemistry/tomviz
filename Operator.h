/******************************************************************************

  This source file is part of the TEM tomography project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#ifndef __TEM_Operator_h
#define __TEM_Operator_h

#include <QObject>
#include <QIcon>

class vtkDataObject;

namespace TEM
{

class Operator : public QObject
{
  Q_OBJECT;
  typedef QObject Superclass;
public:
  Operator(QObject* parent=NULL);
  virtual ~Operator();

  /// Returns a label for this operator.
  virtual QString label() const = 0;

  /// Returns an icon to use for this operator.
  virtual QIcon icon() const = 0;

  /// Method to transform a dataset in-place.
  virtual bool transform(vtkDataObject* data)=0;

private:
  Q_DISABLE_COPY(Operator);
};

}

#endif

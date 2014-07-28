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
#ifndef __TEM_OperatorPython_h
#define __TEM_OperatorPython_h

#include "Operator.h"
#include <QScopedPointer>

namespace TEM
{
class OperatorPython : public Operator
{
  Q_OBJECT
  typedef Operator Superclass;

public:
  OperatorPython(QObject* parent=NULL);
  virtual ~OperatorPython();

  virtual QString label() const { return this->Label; }
  void setLabel(const QString& txt)
    { this->Label = txt; }

  /// Returns an icon to use for this operator.
  virtual QIcon icon() const;

  /// Method to transform a dataset in-place.
  virtual bool transform(vtkDataObject* data);

  /// return a new clone.
  virtual Operator* clone() const;

  virtual bool serialize(pugi::xml_node& in) const;
  virtual bool deserialize(const pugi::xml_node& ns);

  void setScript(const QString& str);
  const QString& script() const { return this->Script; }

private:
  Q_DISABLE_COPY(OperatorPython)

  class OPInternals;
  const QScopedPointer<OPInternals> Internals;
  QString Label;
  QString Script;
};

}
#endif

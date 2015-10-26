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
#ifndef tomvizOperator_h
#define tomvizOperator_h

#include <QObject>
#include <QIcon>
#include <vtk_pugixml.h>

class vtkDataObject;
class QWidget;

namespace tomviz
{
class EditOperatorWidget;

class Operator : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  Operator(QObject* parent=nullptr);
  virtual ~Operator();

  /// Returns a label for this operator.
  virtual QString label() const = 0;

  /// Returns an icon to use for this operator.
  virtual QIcon icon() const = 0;

  /// Method to transform a dataset in-place.
  virtual bool transform(vtkDataObject* data)=0;

  /// Return a new clone.
  virtual Operator* clone() const = 0;

  /// Save/Restore state.
  virtual bool serialize(pugi::xml_node& in) const=0;
  virtual bool deserialize(const pugi::xml_node& ns)=0;

  virtual EditOperatorWidget* getEditorContents(QWidget* parent) = 0;

signals:
  /// fire this signal with the operation is updated/modified
  /// implying that the data needs to be reprocessed.
  void transformModified();
  /// fire this signal to indicate that the operator's label changed
  /// and the GUI needs to refresh its display of the Operator
  void labelModified();

private:
  Q_DISABLE_COPY(Operator)
};

}

#endif

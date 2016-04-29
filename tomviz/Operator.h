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

  bool transform(vtkDataObject *data);

  /// Return a new clone.
  virtual Operator* clone() const = 0;

  /// Save/Restore state.
  virtual bool serialize(pugi::xml_node& in) const = 0;
  virtual bool deserialize(const pugi::xml_node& ns) = 0;

  virtual EditOperatorWidget* getEditorContents(QWidget* parent) = 0;
  virtual bool hasCustomUI() const { return false; }

  /// If the operator has some custom progress UI, then return that UI from this
  /// function.  This custom UI must be parented to the given widget and should have
  /// its signal/slot connections set up to show the progress of the operator.
  /// If there is no need for custom progress UI, then leave this default implementation
  /// and a default QProgressBar will be created instead.
  virtual QWidget* getCustomProgressWidget(QWidget*) const { return nullptr; }
  /// Return the total number of progress updates (assuming each update increments
  /// the progress from 0 to some maximum.  If the operator doesn't support
  /// incremental progress updates, leave this default implementation so that
  /// QProgressBar interprets the progress as unknown.
  virtual int totalProgressSteps() const { return 0; }

signals:
  /// Emit this signal with the operation is updated/modified
  /// implying that the data needs to be reprocessed.
  void transformModified();
  /// Emit this signal to indicate that the operator's label changed
  /// and the GUI needs to refresh its display of the Operator.
  void labelModified();

  /// Returns the number of steps the operator has finished.  The total number of
  /// steps is returned by totalProgressSteps and should be emitted as a finished
  /// signal when all work in the operator is done.
  void updateProgress(int);
  /// Emitted when the operator starts transforming the data
  void transformingStarted();
  /// Emitted when the operator is done transforming the data.  The parameter is the
  /// return value from the transform() function.  True for success, false for failure.
  void transformingDone(bool result);

protected:
  /// Method to transform a dataset in-place.
  virtual bool applyTransform(vtkDataObject* data) = 0;

private:
  Q_DISABLE_COPY(Operator)
};

}

#endif

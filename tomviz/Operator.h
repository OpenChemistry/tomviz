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

#include <QIcon>
#include <QObject>

#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtk_pugixml.h>

class vtkDataObject;
class vtkImageData;
class QWidget;

namespace tomviz {
class DataSource;
class EditOperatorWidget;
class OperatorResult;

enum class OperatorState {QUEUED, RUNNING, COMPLETE, CANCELED, ERROR};

class Operator : public QObject
{
  Q_OBJECT

public:
  Operator(QObject* parent = nullptr);
  ~Operator() override;

  /// Returns the data source the operator operates on
  DataSource* dataSource();

  /// Returns a label for this operator.
  virtual QString label() const = 0;

  /// Returns an icon to use for this operator.
  virtual QIcon icon() const = 0;

  bool transform(vtkDataObject* data);

  /// Return a new clone.
  virtual Operator* clone() const = 0;

  /// Set the number of results produced by this operator.
  virtual void setNumberOfResults(int n);

  /// Get number of output results
  virtual int numberOfResults() const;

  /// Set the result at the given index to the object.
  virtual bool setResult(int index, vtkDataObject* object);

  /// Set the result with the given name to the object.
  virtual bool setResult(const char* name, vtkDataObject* object);

  /// Get output result at index.
  virtual OperatorResult* resultAt(int index) const;

  /// Set whether the operator is expected to produce a child DataSource.
  virtual void setHasChildDataSource(bool value);

  /// Get whether the operator is expected to produce a child DataSource.
  virtual bool hasChildDataSource() const;

  /// Set the child DataSource. Can be nullptr.
  virtual void setChildDataSource(DataSource* source);

  /// Get the child DataSource.
  virtual DataSource* childDataSource() const;

  /// Save/Restore state.
  virtual bool serialize(pugi::xml_node& in) const = 0;
  virtual bool deserialize(const pugi::xml_node& ns) = 0;

  /// There are two versions of this function, this one and
  /// getEditorContentsWithData. Subclasses should override this one if their
  /// editors do not need the previous state of the data.  Subclasses should
  /// override the other if they need the data just prior to this Operator for
  /// the widget to display correctly.  If this data is needed, the default
  /// implementation to return nullptr should be left for this function.
  virtual EditOperatorWidget* getEditorContents(QWidget* vtkNotUsed(parent))
  {
    return nullptr;
  }

  /// Should return a widget for editing customizable parameters on this
  /// operator or nullptr if there is nothing to edit.  The vtkImageData
  /// is a copy of the DataSource's image with all Operators prior in the
  /// pipeline applied to it.  This should be used if the widget needs to
  /// display the VTK data, but modifications to it will not affect the
  /// DataSource.
  virtual EditOperatorWidget* getEditorContentsWithData(
    QWidget* parent,
    vtkSmartPointer<vtkImageData> vtkNotUsed(inputDataForDisplay))
  {
    return this->getEditorContents(parent);
  }

  /// Should return true if the Operator has a non-null widget to return from
  /// getEditorContents.
  virtual bool hasCustomUI() const { return false; }

  /// If the operator has some custom progress UI, then return that UI from this
  /// function.  This custom UI must be parented to the given widget and should
  /// have its signal/slot connections set up to show the progress of the
  /// operator. If there is no need for custom progress UI, then leave this
  /// default implementation and a default QProgressBar will be created instead.
  virtual QWidget* getCustomProgressWidget(QWidget*) const { return nullptr; }

  /// Returns true if the operation supports canceling midway through the
  /// applyTransform function via the cancelTransform slot.  Defaults to false,
  /// can be set by the setSupportsCancel(bool) method by subclasses.
  bool supportsCancelingMidTransform() const { return m_supportsCancel; }

  /// Return the total number of progress updates (assuming each update
  /// increments the progress from 0 to some maximum.  If the operator doesn't
  /// support incremental progress updates, leave value set to zero
  /// that QProgressBar interprets the progress as unknown.
  int totalProgressSteps() const { return m_totalProgressSteps; }

  void setTotalProgressSteps(int steps) { m_totalProgressSteps = steps; }

signals:
  /// Emit this signal with the operation is updated/modified
  /// implying that the data needs to be reprocessed.
  void transformModified();

  /// Emit this signal to indicate that the operator's label changed
  /// and the GUI needs to refresh its display of the Operator.
  void labelModified();

  /// Returns the number of steps the operator has finished.  The total number
  /// of steps is returned by totalProgressSteps and should be emitted as a
  /// finished signal when all work in the operator is done.
  void updateProgress(int);

  /// Emitted when the operator starts transforming the data
  void transformingStarted();

  /// Emitted when the operator is done transforming the data.  The parameter is
  /// the return value from the transform() function.  True for success, false
  /// for failure.
  void transformingDone(bool result);

  /// Emitted when an result is added.
  void resultAdded(OperatorResult* result);

  /// Emitted when the total progress steps has changed.
  void totalProgressStepsChanged(int steps);

public slots:
  /// Called when the 'Cancel' button is pressed on the progress dialog.
  /// Subclasses overriding this method should call the base implementation
  /// to ensure the operator is marked as canceled.
  virtual void cancelTransform() { m_state = OperatorState::CANCELED; };
  bool isCanceled() { return m_state == OperatorState::CANCELED; };
  bool isFinished() { return m_state == OperatorState::COMPLETE; };
  OperatorState state() { return m_state; };
  void resetState() { m_state = OperatorState::QUEUED; };

protected:
  /// Method to transform a dataset in-place.
  virtual bool applyTransform(vtkDataObject* data) = 0;

  /// Method to set whether the operator supports canceling midway through the
  /// transform method call.  If you set this to true, you should also override
  /// the cancelTransform slot to listen for the cancel signal and handle it.
  void setSupportsCancel(bool b) { m_supportsCancel = b; }

private:
  Q_DISABLE_COPY(Operator)

  QList<OperatorResult*> m_results;
  bool m_supportsCancel = false;
  bool m_hasChildDataSource = false;
  DataSource* m_childDataSource = nullptr;
  int m_totalProgressSteps = 0;
  OperatorState m_state = OperatorState::QUEUED;
};
}

#endif

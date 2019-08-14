/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperator_h
#define tomvizOperator_h

#include <atomic>

#include <QIcon>
#include <QObject>
#include <QPointer>

#include <vtkObject.h>
#include <vtkSmartPointer.h>
#include <vtk_pugixml.h>

#include "DataSource.h"

class vtkDataObject;
class vtkImageData;
class QWidget;

Q_DECLARE_METATYPE(vtkSmartPointer<vtkDataObject>)

namespace tomviz {
class EditOperatorWidget;
class OperatorResult;
class EditOperatorDialog;

enum class OperatorState
{
  Queued,
  Running,
  Complete,
  Canceled,
  Error,
  Edit
};

enum class TransformResult
{
  Complete = static_cast<int>(OperatorState::Complete),
  Canceled = static_cast<int>(OperatorState::Canceled),
  Error = static_cast<int>(OperatorState::Error)
};

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

  TransformResult transform(vtkDataObject* data);

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
  virtual QJsonObject serialize() const;
  virtual bool deserialize(const QJsonObject& json);

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
    QWidget* parent_,
    vtkSmartPointer<vtkImageData> vtkNotUsed(inputDataForDisplay))
  {
    return this->getEditorContents(parent_);
  }

  /// Should return true if the Operator has a non-null widget to return from
  /// getEditorContents.
  virtual bool hasCustomUI() const { return false; }

  /// If this operator has a dialog active, this should return that dialog (the
  /// dialog will register itself using setCustomDialog in its constructor).
  /// Otherwise this will return nullptr.
  EditOperatorDialog* customDialog() const;

  /// Set the custom dialog associated with this operator
  void setCustomDialog(EditOperatorDialog*);

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

  /// Set the total number of progress steps
  void setTotalProgressSteps(int steps)
  {
    m_totalProgressSteps = steps;
    emit totalProgressStepsChanged(steps);
  }

  /// Returns the current progress step
  int progressStep() const { return m_progressStep; }

  /// Set the current progress step
  void setProgressStep(int step)
  {
    m_progressStep = step;
    emit progressStepChanged(step);
  }

  /// Returns the current progress message
  QString progressMessage() const { return m_progressMessage; }

  /// Set the current progress message which will appear in the progress dialog
  /// title.
  void setProgressMessage(const QString& message)
  {
    m_progressMessage = message;
    emit progressMessageChanged(message);
  }

  /// Set the operator state, this is needed for external execution.
  void setState(OperatorState state) { m_state = state; }

signals:
  /// Emit this signal with the operation is updated/modified
  /// implying that the data needs to be reprocessed.
  void transformModified();

  /// Emit this signal to indicate that the operator's label changed
  /// and the GUI needs to refresh its display of the Operator.
  void labelModified();

  /// Emitted to indicate that the progress step has changed.
  void progressStepChanged(int);

  /// Emitted to indicate that the progress message has changed.
  void progressMessageChanged(const QString& message);

  /// Emitted when the operator starts transforming the data
  void transformingStarted();

  /// Emitted when the operator is done transforming the data.  The parameter is
  /// the return value from the transform() function.  True for success, false
  /// for failure.
  void transformingDone(TransformResult result);

  /// Emitted when an result is added.
  void resultAdded(OperatorResult* result);

  /// Emitted when the total progress steps has changed.
  void totalProgressStepsChanged(int steps);

  /// Emitted when a child data source is create by this operator.
  void newChildDataSource(DataSource*);

  // Signal used to request the creation of a new data source. Needed to
  // ensure the initialization of the new DataSource is performed on UI thread
  void newChildDataSource(const QString&, vtkSmartPointer<vtkDataObject>);

  /// Emitted just prior to this object's destruction.
  void aboutToBeDestroyed(Operator* op);

  // Emitted when a data source is move to a new operator. For example when
  // a new operator is added.
  void dataSourceMoved(DataSource*);

  // Emitted when the operator is moved into the canceled state. Note at this
  // point the operator may still be running. This is used to indicate that
  // a request to cancel this operator has been issued.
  void transformCanceled();

public slots:
  /// Called when the 'Cancel' button is pressed on the progress dialog.
  /// Subclasses overriding this method should call the base implementation
  /// to ensure the operator is marked as canceled.
  /// TODO: Not sure we need this/want this virtual anymore?
  virtual void cancelTransform();
  bool isCanceled() { return m_state == OperatorState::Canceled; }
  bool isFinished()
  {
    return m_state == OperatorState::Complete ||
           m_state == OperatorState::Error;
  };
  bool isModified() { return m_modified; }
  bool isNew() { return m_new; }
  bool isEditing() { return m_state == OperatorState::Edit; }
  bool isQueued() { return m_state == OperatorState::Queued; }

  OperatorState state() { return m_state; }
  void setModified() { m_modified = true; }
  void resetState() { m_state = OperatorState::Queued; }
  void setEditing() { m_state = OperatorState::Edit; }
  void setComplete() { m_state = OperatorState::Complete; }

protected slots:
  // Create a new child datasource and set it on this operator
  void createNewChildDataSource(
    const QString& label, vtkSmartPointer<vtkDataObject>,
    DataSource::DataSourceType type = DataSource::DataSourceType::Volume,
    DataSource::PersistenceState state =
      DataSource::PersistenceState::Transient);

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
  bool m_modified = true;
  bool m_new = true;
  QPointer<DataSource> m_childDataSource;
  int m_totalProgressSteps = 0;
  int m_progressStep = 0;
  QString m_progressMessage;
  std::atomic<OperatorState> m_state{ OperatorState::Queued };
  QPointer<EditOperatorDialog> m_customDialog;
};
} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizOperatorResult_h
#define tomvizOperatorResult_h

#include <QObject>
#include <QString>

#include <vtkWeakPointer.h>

class vtkDataObject;
class vtkSMSourceProxy;

namespace tomviz {

// Output result from an operator. Such results may include label maps or
// tables. This class wraps a single vtkDataObject produced by an operator.
class OperatorResult : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  OperatorResult(QObject* parent = nullptr);
  virtual ~OperatorResult() override;

  /// Set name of object.
  void setName(QString name);
  QString name() const;

  /// Set label of object.
  void setLabel(QString label);
  QString label() const;

  /// Set description of object.
  void setDescription(QString desc);
  QString description() const;

  /// Clean up object, releasing the data object and the proxy created
  /// for it.
  virtual bool finalize();

  /// Get and set the data object this result wraps.
  virtual vtkDataObject* dataObject();
  virtual void setDataObject(vtkDataObject* dataObject);

  virtual vtkSMSourceProxy* producerProxy();

private:
  Q_DISABLE_COPY(OperatorResult)

  vtkWeakPointer<vtkSMSourceProxy> m_producerProxy;
  QString m_name;
  QString m_label;
  QString m_description = "Operator Result";

  void createProxyIfNeeded();
  void deleteProxy();
};

} // namespace tomviz

#endif

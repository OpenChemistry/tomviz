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
  void setName(const QString& name);
  const QString& name();

  /// Set label of object.
  void setLabel(const QString& label);
  const QString& label();

  /// Set description of object.
  void setDescription(const QString& desc);
  const QString& description();

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

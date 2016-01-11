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
#ifndef tomvizProgressBehavior_h
#define tomvizProgressBehavior_h

#include <QObject>
#include <QPointer>

class QProgressDialog;
class QWidget;

namespace tomviz
{

/// Behavior to show a progress dialog for ParaView progress events.
class ProgressBehavior : public QObject
{
  Q_OBJECT
  typedef QObject Superclass;

public:
  ProgressBehavior(QWidget* parent = nullptr);
  ~ProgressBehavior();

  /// Delayed initialization of the dialog until it is used.
  void initialize();

private slots:
  void enableProgress(bool enable);
  void progress(const QString& message, int progress);

private:
  Q_DISABLE_COPY(ProgressBehavior)
  QPointer<QProgressDialog> ProgressDialog;
};
}
#endif

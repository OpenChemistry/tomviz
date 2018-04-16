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
#ifndef tomvizEditOperatorWidget_h
#define tomvizEditOperatorWidget_h

#include <QWidget>

namespace tomviz {
// This class is the GUI needed to edit the properties of an operator.  The
// operator will return one of these from its getEditorContents and it will
// be shown in a dialog.  When Apply or Ok is clicked on the dialog, the
// applyChangesToOperator() slot will be called on the widget.
class EditOperatorWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  EditOperatorWidget(QWidget* parent);
  ~EditOperatorWidget();

  // Called when the user interacts to move the data source while the widget
  // is active. By default this emits the signal which can be attached to
  // subwidgets'  slots, but it can be overridden for custom handling.
  virtual void dataSourceMoved(double newX, double newY, double newZ)
  {
    emit dataMoved(newX, newY, newZ);
  }

  // Used to set the mode of the EditOperatorWidget.  The mode corresponds to
  // options like tabs that change the whole widget's appearance and varies from
  // operator to operator.  If the requested mode is not recognized, or the
  // widget does not support modes, this function does nothing.  The default
  // implementation of this function does nothing.  This method should be
  // overridden to add support for modes.
  virtual void setViewMode(const QString&) {}

signals:
  void dataMoved(double, double, double);

public slots:
  // Called when the dialog should apply its changes to the operator
  virtual void applyChangesToOperator() = 0;
};
}

#endif

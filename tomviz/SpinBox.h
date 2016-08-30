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
#ifndef tomvizSpinBox_h
#define tomvizSpinBox_h

#include <QSpinBox>

/*
 * This class is a QSpinBox that fires its editingFinished() signal whenever the
 * value is modified from the up and down arrow buttons in addition to when it
 * loses focus.  We want to update in response to both of these.
 */

namespace tomviz {

class SpinBox : public QSpinBox
{
  Q_OBJECT
public:
  SpinBox(QWidget* parent = nullptr);

protected:
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);

private:
  bool pressInUp;
  bool pressInDown;
};
}

#endif

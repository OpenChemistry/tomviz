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
#ifndef tomvizCustomPythonOperatorWidget_h
#define tomvizCustomPythonOperatorWidget_h

#include <QWidget>

namespace tomviz {

class CustomPythonOperatorWidget : public QWidget
{
  Q_OBJECT

public:
  CustomPythonOperatorWidget(QWidget* parent);
  virtual ~CustomPythonOperatorWidget();

  virtual void getValues(QMap<QString, QVariant>& map) = 0;
  virtual void setValues(const QMap<QString, QVariant>& map) = 0;
};
}

#endif

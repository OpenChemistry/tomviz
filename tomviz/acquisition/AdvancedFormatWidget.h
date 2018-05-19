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
#ifndef tomvizAdvancedFormatWidget_h
#define tomvizAdvancedFormatWidget_h

#include <QWidget>

#include <QScopedPointer>


namespace Ui {
class AdvancedFormatWidget;
}

namespace tomviz {
class AdvancedFormatWidget : public QWidget {
  Q_OBJECT

  public:
    AdvancedFormatWidget(QWidget* parent = nullptr);
    ~AdvancedFormatWidget() override;

  public slots:

  private slots:

  signals:

  private:
    QScopedPointer<Ui::AdvancedFormatWidget> m_ui;
};

}

#endif

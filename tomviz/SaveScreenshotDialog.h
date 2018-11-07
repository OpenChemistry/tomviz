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
#ifndef tomvizSaveScreenshotDialog_h
#define tomvizSaveScreenshotDialog_h

#include <QDialog>

class QComboBox;
class QSpinBox;

namespace tomviz {

class SaveScreenshotDialog : public QDialog
{
  Q_OBJECT

public:
  explicit SaveScreenshotDialog(QWidget* parent = nullptr);
  ~SaveScreenshotDialog() override;

  void setSize(int w, int h);
  int width() const;
  int height() const;

  void addPalette(const QString& name, const QString& key);
  QString palette() const;

private slots:
  void setLockAspectRatio();
  void widthChanged(int i);
  void heightChanged(int i);

private:
  bool m_lockAspectRatio = false;
  double m_aspectRatio = 1.0;

  QSpinBox *m_width = nullptr;
  QSpinBox *m_height = nullptr;
  QComboBox *m_palettes = nullptr;
};
} // namespace tomviz

#endif

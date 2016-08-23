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
#ifndef tomvizIntSliderWidget_h
#define tomvizIntSliderWidget_h

#include <QWidget>

class QSlider;
class pqLineEdit;

namespace tomviz
{

class IntSliderWidget : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue USER true)
  Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
  Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
  Q_PROPERTY(bool strictRange READ strictRange WRITE setStrictRange)

public:
  IntSliderWidget(bool showLineEdit, QWidget *parent = nullptr);
  ~IntSliderWidget();

  int value() const;

  int minimum() const;
  int maximum() const;

  bool strictRange() const;

  void setLineEditWidth(int width);
  void setPageStep(int step);

signals:
  void valueChanged(int);
  void valueEdited(int);

public slots:
  void setValue(int);
  void setMinimum(int);
  void setMaximum(int);
  void setStrictRange(bool);

private slots:
  void sliderChanged(int);
  void textChanged(const QString&);
  void editingFinished();
  void updateValidator();
  void updateSlider();

private:
  int Value;
  int Minimum;
  int Maximum;
  QSlider *Slider;
  pqLineEdit *LineEdit;
  bool StrictRange;
  bool BlockUpdate;
};

}
#endif


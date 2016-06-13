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
#ifndef tomvizDoubleSliderWidget_h
#define tomvizDoubleSliderWidget_h

#include <QWidget>

class QSlider;
class pqLineEdit;

namespace tomviz
{

class DoubleSliderWidget : public QWidget
{
  Q_OBJECT
  Q_PROPERTY(double value READ value WRITE setValue USER true)
  Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
  Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
  Q_PROPERTY(bool strictRange READ strictRange WRITE setStrictRange)
  Q_PROPERTY(int resolution READ resolution WRITE setResolution)

public:
  DoubleSliderWidget(bool showLineEdit, QWidget *parent = nullptr);
  ~DoubleSliderWidget();

  double value() const;

  double minimum() const;
  double maximum() const;

  bool strictRange() const;

  int resolution() const;

signals:
  void valueChanged(double);
  void valueEdited(double);

public slots:
  void setValue(double);
  void setMinimum(double);
  void setMaximum(double);
  void setStrictRange(bool);
  void setResolution(int);

private slots:
  void sliderChanged(int);
  void textChanged(const QString&);
  void editingFinished();
  void updateValidator();
  void updateSlider();

private:
  int Resolution;
  double Value;
  double Minimum;
  double Maximum;
  QSlider *Slider;
  pqLineEdit *LineEdit;
  bool StrictRange;
  bool BlockUpdate;
};

}
#endif

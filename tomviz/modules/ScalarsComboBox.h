#ifndef tomvizScalarsPicker_h
#define tomvizScalarsPicker_h

#include <QComboBox>

namespace tomviz {

class DataSource;
class Module;

class ScalarsComboBox : public QComboBox
{
  Q_OBJECT

public:
  ScalarsComboBox(QWidget* parent = nullptr);
  void setOptions(DataSource* ds, Module* module);
};
}

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizHdf5SubsampleWidget_h
#define tomvizHdf5SubsampleWidget_h

#include <QScopedPointer>
#include <QWidget>

namespace tomviz {

class Hdf5SubsampleWidget : public QWidget
{
  Q_OBJECT
  typedef QWidget Superclass;

public:
  Hdf5SubsampleWidget(int dims[3], int dataTypeSize, QWidget* parent = nullptr);
  virtual ~Hdf5SubsampleWidget();

  // For setting defaults
  void setStrides(int s[3]);
  void setBounds(int bs[6]);

  // For getting the results
  void bounds(int bs[6]) const;
  void strides(int s[3]) const;

private slots:
  void valueChanged();

private:
  class Internals;
  QScopedPointer<Internals> m_internals;
};
} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPyXRFMakeHDF5Dialog_h
#define tomvizPyXRFMakeHDF5Dialog_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class PyXRFMakeHDF5Dialog : public QDialog
{
  Q_OBJECT

public:
  explicit PyXRFMakeHDF5Dialog(QWidget* parent);
  ~PyXRFMakeHDF5Dialog() override;

  void show();

  QString command() const;
  bool useAlreadyExistingData() const;
  QString workingDirectory() const;
  int scanStart() const;
  int scanStop() const;
  bool successfulScansOnly() const;
  bool remakeCsvFile() const;

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};
} // namespace tomviz

#endif

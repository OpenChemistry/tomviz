/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPyXRFProcessDialog_h
#define tomvizPyXRFProcessDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class PyXRFProcessDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PyXRFProcessDialog(QString workingDirectory, QWidget* parent);
  ~PyXRFProcessDialog() override;

  virtual void show();

  QString command() const;
  QString parametersFile() const;
  QString logFile() const;
  QString icName() const;
  QString outputDirectory() const;
  double pixelSizeX() const;
  double pixelSizeY() const;
  bool skipProcessed() const;
  bool rotateDatasets() const;

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz

#endif

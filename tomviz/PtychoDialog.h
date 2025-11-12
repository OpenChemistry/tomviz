/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPtychoDialog_h
#define tomvizPtychoDialog_h

#include <QDialog>
#include <QScopedPointer>

namespace tomviz {

class PtychoDialog : public QDialog
{
  Q_OBJECT

public:
  explicit PtychoDialog(QWidget* parent);
  ~PtychoDialog() override;

  virtual void show();

  void clearTable();
  void updateTableData(long minSID, long maxSID, QStringList versionList);

  QString ptychoDirectory() const;
  QMap<QString, QStringList> loadSIDSettings() const;
  QString outputDirectory() const;
  bool rotateDatasets() const;

signals:
  void needTableDataUpdate();

private:
  class Internal;
  QScopedPointer<Internal> m_internal;
};

} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataBrokerLoadDialog_h
#define tomvizDataBrokerLoadDialog_h

#include <QDialog>

#include <QDate>
#include <QList>
#include <QScopedPointer>
#include <QVariantMap>

namespace Ui {
class DataBrokerLoadDialog;
}

class QTreeWidgetItem;

namespace tomviz {

class DataBroker;
class ListResourceCall;

class DataBrokerLoadDialog : public QDialog
{
  Q_OBJECT

public:
  explicit DataBrokerLoadDialog(DataBroker* dataBroker,
                                QWidget* parent = nullptr);
  ~DataBrokerLoadDialog() override;

  QString selectedCatalog();
  QString selectedRunUid();
  QString selectedTable();
  QString selectedVariable();

private:
  Q_DISABLE_COPY(DataBrokerLoadDialog)
  QScopedPointer<Ui::DataBrokerLoadDialog> m_ui;
  DataBroker* m_dataBroker;
  QList<QVariantMap> m_catalogs;
  QList<QVariantMap> m_runs;
  QList<QVariantMap> m_tables;
  QList<QVariantMap> m_variables;

  QString m_selectedCatalog = "fxi";
  QString m_selectedRunUid;
  QString m_selectedTable = "primary";
  QString m_selectedVariable = "Andor_image";
  QDate m_fromDate;
  QDate m_toDate;
  int m_id = -1;
  bool m_dateFilter;
  bool m_idFilter;
  int m_limit = 20;

  void loadCatalogs();
  void loadRuns(const QString& catalog, int id, bool dateFilter,
                const QDate& fromDate, const QDate& toDate, int limit);
  void loadTables(const QString& catalog, const QString& runUid);
  void loadVariables(const QString& catalog, const QString& runUid,
                     const QString& table);

  void showCatalogs();
  void showRuns();
  void showTables();
  void showVariables();
  void setLabel(const QString& label);
  void setEnabledResetButton(bool enable);
  void setEnabledOkButton(bool enable);
  void allowFilter(bool allow);
  void beginCall();
  void endCall();
  void setErrorMessage(const QString& errorMessage);
  void clearErrorMessage();
  void connectErrorSignal(ListResourceCall* call);

private slots:
  void enableFilterByDate(bool enable);
  void enableFilterByID(bool enable);
  void applyFilter();
};
} // namespace tomviz

#endif

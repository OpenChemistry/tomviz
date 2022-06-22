/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizDataBrokerSaveDialog_h
#define tomvizDataBrokerSaveDialog_h

#include "ActiveObjects.h"

#include <QDialog>

#include <QDate>
#include <QList>
#include <QScopedPointer>
#include <QVariantMap>

namespace Ui {
class DataBrokerSaveDialog;
}

class QTreeWidgetItem;

namespace tomviz {

class DataBroker;
class ListResourceCall;

class DataBrokerSaveDialog : public QDialog
{
  Q_OBJECT

public:
  explicit DataBrokerSaveDialog(DataBroker* dataBroker,
                                QWidget* parent = nullptr);
  ~DataBrokerSaveDialog() override;
  QString name();

private:
  Q_DISABLE_COPY(DataBrokerSaveDialog)
  QScopedPointer<Ui::DataBrokerSaveDialog> m_ui;
  DataBroker* m_dataBroker;

  void setEnabledOkButton(bool enable);

  QString m_name;
};
} // namespace tomviz

#endif

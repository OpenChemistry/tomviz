/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizCustomFormatWidget_h
#define tomvizCustomFormatWidget_h

#include <QWidget>

#include <QScopedPointer>

namespace Ui {
class CustomFormatWidget;
}

namespace tomviz {

class CustomFormatWidget : public QWidget
{
  Q_OBJECT

public:
  CustomFormatWidget(QWidget* parent = nullptr);
  ~CustomFormatWidget() override;
  QStringList getFields() const;
  void setFields(const QStringList&);
  void setAllowEdit(bool);

signals:
  void fieldsChanged();

private slots:
  void onPrefixChanged(QString prefix);
  void onNegChanged(QString prefix);
  void onPosChanged(QString prefix);
  void onSuffixChanged(QString prefix);
  void onExtensionChanged(QString prefix);

private:
  QScopedPointer<Ui::CustomFormatWidget> m_ui;
  QString m_prefix = "*";
  QString m_suffix = "*";
  QString m_ext = "*";
  QString m_negChar = "n";
  QString m_posChar = "p";
};
}

#endif

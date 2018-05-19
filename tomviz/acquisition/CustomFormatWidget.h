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
    void setFields(QString, QString, QString, QString, QString);
    void setAllowEdit(bool);

  signals:
    void fieldsChanged();

  protected:


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

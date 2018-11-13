/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "CustomFormatWidget.h"

#include "ui_CustomFormatWidget.h"

namespace tomviz {

CustomFormatWidget::CustomFormatWidget(QWidget* parent)
  : QWidget(parent), m_ui(new Ui::CustomFormatWidget)
{
  m_ui->setupUi(this);
  m_ui->prefixEdit->setText(m_prefix);
  m_ui->suffixEdit->setText(m_suffix);
  m_ui->extensionEdit->setText(m_ext);
  m_ui->negativeEdit->setText(m_negChar);
  m_ui->positiveEdit->setText(m_posChar);

  connect(m_ui->prefixEdit, &QLineEdit::textEdited, this,
          &CustomFormatWidget::onPrefixChanged);
  connect(m_ui->suffixEdit, &QLineEdit::textEdited, this,
          &CustomFormatWidget::onSuffixChanged);
  connect(m_ui->extensionEdit, &QLineEdit::textEdited, this,
          &CustomFormatWidget::onExtensionChanged);
  connect(m_ui->negativeEdit, &QLineEdit::textEdited, this,
          &CustomFormatWidget::onNegChanged);
  connect(m_ui->positiveEdit, &QLineEdit::textEdited, this,
          &CustomFormatWidget::onPosChanged);
}

CustomFormatWidget::~CustomFormatWidget() = default;

void CustomFormatWidget::onPrefixChanged(QString val)
{
  m_prefix = val;
  emit fieldsChanged();
}

void CustomFormatWidget::onNegChanged(QString val)
{
  m_negChar = val;
  emit fieldsChanged();
}

void CustomFormatWidget::onPosChanged(QString val)
{
  m_posChar = val;
  emit fieldsChanged();
}

void CustomFormatWidget::onSuffixChanged(QString val)
{
  m_suffix = val;
  emit fieldsChanged();
}

void CustomFormatWidget::onExtensionChanged(QString val)
{
  m_ext = val;
  emit fieldsChanged();
}

QStringList CustomFormatWidget::getFields() const
{
  QStringList fields;
  fields << m_prefix << m_negChar << m_posChar << m_suffix << m_ext;
  return fields;
}

void CustomFormatWidget::setFields(const QStringList& fields)
{
  if (fields.size() != 5) {
    return;
  }
  m_prefix = fields[0];
  m_negChar = fields[1];
  m_posChar = fields[2];
  m_suffix = fields[3];
  m_ext = fields[4];

  m_ui->prefixEdit->setText(m_prefix);
  m_ui->suffixEdit->setText(m_suffix);
  m_ui->extensionEdit->setText(m_ext);
  m_ui->negativeEdit->setText(m_negChar);
  m_ui->positiveEdit->setText(m_posChar);
}

void CustomFormatWidget::setAllowEdit(bool allow)
{
  m_ui->prefixEdit->setEnabled(allow);
  m_ui->suffixEdit->setEnabled(allow);
  m_ui->extensionEdit->setEnabled(allow);
  m_ui->negativeEdit->setEnabled(allow);
  m_ui->positiveEdit->setEnabled(allow);
}

} // namespace tomviz

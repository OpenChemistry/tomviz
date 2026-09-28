/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "EnumOptions.h"

#include <QJsonObject>
#include <QMetaType>

namespace tomviz {
namespace pipeline {
namespace PythonNodeUtils {

namespace {

QVariant optionValueAt(const QJsonArray& options, int idx)
{
  if (idx < 0 || idx >= options.size()) {
    return QVariant();
  }
  QJsonObject opt = options.at(idx).toObject();
  return opt.isEmpty() ? QVariant() : opt.constBegin().value().toVariant();
}

} // namespace

int enumOptionIndex(const QVariant& value, const QJsonArray& options)
{
  if (!value.isValid()) {
    return -1;
  }
  const bool numeric = value.canConvert<double>() &&
                       value.typeId() != QMetaType::QString;
  for (int i = 0; i < options.size(); ++i) {
    QVariant option = optionValueAt(options, i);
    if (!option.isValid()) {
      continue;
    }
    const bool optionNumeric = option.typeId() != QMetaType::QString;
    if (numeric && optionNumeric) {
      if (option.toDouble() == value.toDouble()) {
        return i;
      }
    } else if (!numeric && !optionNumeric) {
      if (option.toString() == value.toString()) {
        return i;
      }
    }
  }
  return -1;
}

QVariant resolveEnumValue(const QJsonValue& value, const QJsonArray& options)
{
  const int matched = enumOptionIndex(value.toVariant(), options);
  if (matched >= 0) {
    return optionValueAt(options, matched);
  }
  if (value.isString()) {
    return value.toVariant();
  }
  if (value.isDouble()) {
    return optionValueAt(options, value.toInt());
  }
  return QVariant();
}

QVariant resolveEnumDefault(const QJsonValue& value,
                            const QJsonArray& options)
{
  if (value.isDouble()) {
    QVariant byIndex = optionValueAt(options, value.toInt());
    if (byIndex.isValid()) {
      return byIndex;
    }
  }
  const int matched = enumOptionIndex(value.toVariant(), options);
  if (matched >= 0) {
    return optionValueAt(options, matched);
  }
  return value.isString() ? value.toVariant() : QVariant();
}

} // namespace PythonNodeUtils
} // namespace pipeline
} // namespace tomviz

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "LabelSelection.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace tomviz {

QVector<double> parseLabelList(const QString& text)
{
  static const QRegularExpression separators("[,;\\s]+");
  static const QRegularExpression spacedRun("(\\d+)\\s*-\\s*(\\d+)");
  static const QRegularExpression run("^(\\d+)-(\\d+)$");
  QVector<double> labels;
  // "3 - 5" is one run, so close it up before splitting on whitespace
  QString tidy = text;
  tidy.replace(spacedRun, "\\1-\\2");
  for (const auto& token : tidy.split(separators, Qt::SkipEmptyParts)) {
    auto match = run.match(token);
    if (match.hasMatch()) {
      double first = match.captured(1).toDouble();
      double last = match.captured(2).toDouble();
      if (first > last) {
        std::swap(first, last);
      }
      for (double v = first; v <= last; v += 1.0) {
        labels.append(v);
      }
      continue;
    }
    bool ok = false;
    double value = token.toDouble(&ok);
    if (ok) {
      labels.append(value);
    }
  }
  std::sort(labels.begin(), labels.end());
  labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
  return labels;
}

QString formatLabelList(QVector<double> labels)
{
  std::sort(labels.begin(), labels.end());
  labels.erase(std::unique(labels.begin(), labels.end()), labels.end());

  auto number = [](double v) {
    return v == std::floor(v) ? QString::number(static_cast<qlonglong>(v))
                              : QString::number(v);
  };
  QStringList parts;
  int i = 0;
  while (i < labels.size()) {
    int j = i;
    // Extend over consecutive integers; a run of three or more is worth
    // writing as a range.
    while (j + 1 < labels.size() && labels[j] == std::floor(labels[j]) &&
           labels[j + 1] == labels[j] + 1.0) {
      ++j;
    }
    if (j - i >= 2) {
      parts.append(number(labels[i]) + "-" + number(labels[j]));
    } else {
      for (int k = i; k <= j; ++k) {
        parts.append(number(labels[k]));
      }
    }
    i = j + 1;
  }
  return parts.join(", ");
}

} // namespace tomviz

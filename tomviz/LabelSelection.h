/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLabelSelection_h
#define tomvizLabelSelection_h

#include <QString>
#include <QVector>

namespace tomviz {

/// The label values named by a list such as "3, 5, 10-14": commas,
/// semicolons and whitespace separate entries, and "first-last" is a
/// run. Sorted ascending with duplicates dropped; entries that are not
/// numbers are ignored. The Remove Labels operator reads the same
/// format.
QVector<double> parseLabelList(const QString& text);

/// The list form of @a labels, "3, 5, 10-14", with runs of consecutive
/// integers folded into ranges.
QString formatLabelList(QVector<double> labels);

} // namespace tomviz

#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLabelSelectionWidget_h
#define tomvizLabelSelectionWidget_h

#include "CustomPythonNodeWidget.h"
#include "PortData.h"

#include <QMap>
#include <QVector>

class QCheckBox;
class QLabel;
class QTableWidget;

namespace tomviz {

/// The Remove Labels operator's editor: the labels present in the input,
/// each with a box to tick for removal. Named after the operator's
/// "labels" parameter so a "bindToSink" declaration can link the ticks
/// to the hidden labels of a Label Map visualization on the same data.
class LabelSelectionWidget : public pipeline::CustomPythonNodeWidget
{
  Q_OBJECT

public:
  LabelSelectionWidget(const QMap<QString, pipeline::PortData>& inputs,
                       QWidget* parent = nullptr);

  void getValues(QMap<QString, QVariant>& map) override;
  void setValues(const QMap<QString, QVariant>& map) override;

  /// The labels ticked for removal, ascending. Labels named in the
  /// parameter but absent from the input are kept too, so a saved
  /// selection survives the data changing under it.
  QVector<double> selectedLabels() const;
  /// Tick exactly @a labels. Does not emit selectionChanged().
  void setSelectedLabels(const QVector<double>& labels);

signals:
  /// The user changed which labels are ticked.
  void selectionChanged();

private:
  void setAllChecked(bool checked);
  void invertChecked();
  void updateSummary();

  QTableWidget* m_table = nullptr;
  QCheckBox* m_renumber = nullptr;
  QLabel* m_summary = nullptr;
  /// Labels from the parameter that the input does not hold; carried
  /// along invisibly rather than dropped.
  QVector<double> m_unlisted;
};

} // namespace tomviz

#endif

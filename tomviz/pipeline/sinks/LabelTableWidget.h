/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineLabelTableWidget_h
#define tomvizPipelineLabelTableWidget_h

#include <QAbstractTableModel>
#include <QPointer>
#include <QWidget>

#include <memory>

class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTableView;

namespace tomviz {
namespace pipeline {

class LabelMapData;
class LabelMapSink;

/// Table model over a label map's LabelTable: one row per label,
/// exposing its visibility, color and name for editing.
///
/// Holds the label map weakly. The payload belongs to the output port,
/// which drops it when the pipeline evicts transient data, and a model
/// pinning a whole volume in memory for the sake of a panel would
/// defeat that.
class LabelTableModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  enum Column
  {
    VisibleColumn = 0,
    ColorColumn,
    ValueColumn,
    NameColumn,
    CountColumn,
    ColumnCount
  };

  explicit LabelTableModel(QObject* parent = nullptr);

  void setLabelMap(const std::shared_ptr<LabelMapData>& labelMap);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value,
               int role) override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  /// Set every label's visibility at once, as one edit.
  void setAllVisible(bool visible);
  void invertVisibility();
  /// Assign the color of one row, addressed in source-model terms.
  void setColor(int row, const QColor& color);
  /// The table's visibility flags were changed from outside the model
  /// (setHiddenLabels); refresh the check boxes without a reset.
  void refreshVisibility();

signals:
  /// Emitted after any change to the underlying label table, so the
  /// owner can project it back onto the transfer functions.
  void labelsEdited();

private:
  std::shared_ptr<LabelMapData> lock() const;

  std::weak_ptr<LabelMapData> m_labelMap;
};

/// The label map sink's headline control: the list of labels present in
/// the data, each with a checkbox that shows or hides it and a swatch
/// that recolors it.
class LabelTableWidget : public QWidget
{
  Q_OBJECT

public:
  explicit LabelTableWidget(LabelMapSink* sink, QWidget* parent = nullptr);

  /// Re-read the label table from the sink. Called on construction and
  /// whenever the sink consumes fresh data.
  void refresh();

private:
  void onDoubleClicked(const QModelIndex& proxyIndex);
  void applyEdits();

  QPointer<LabelMapSink> m_sink;
  LabelTableModel* m_model;
  QSortFilterProxyModel* m_proxy;
  QTableView* m_view;
  QLineEdit* m_filter;
  QLabel* m_summary;
};

} // namespace pipeline
} // namespace tomviz

#endif

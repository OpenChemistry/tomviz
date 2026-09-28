/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */
#ifndef tomvizCustomOperatorManagerDialog_h
#define tomvizCustomOperatorManagerDialog_h

#include "PythonUtilities.h"

#include <QDialog>

#include <functional>
#include <vector>

class QGridLayout;
class QLabel;
class QScrollArea;

namespace tomviz {

/// Lists the custom operators the Custom Transforms menu shows, sources
/// first and transforms below, one row each: label, where its files
/// live, and its own Edit, Delete, Clone and open-folder buttons. Clone
/// and open-folder work for every entry; Edit and Delete only for files
/// in the user's own directory, and are disabled with a reason
/// otherwise. Broken definitions are listed too, with their error, since
/// fixing or removing them is the point.
///
/// The dialog only asks and reports: the list comes from a provider the
/// host supplies, and each button emits a request the host acts on. The
/// host calls refresh() once something changed on disk.
class CustomOperatorManagerDialog : public QDialog
{
  Q_OBJECT

public:
  using Provider = std::function<std::vector<OperatorDescription>()>;

  explicit CustomOperatorManagerDialog(Provider provider,
                                       QWidget* parent = nullptr);

  /// Re-query the provider and rebuild the list.
  void refresh();

signals:
  void createRequested();
  void cloneRequested(const OperatorDescription& op);
  void editRequested(const OperatorDescription& op);
  void deleteRequested(const OperatorDescription& op);
  /// Show the directory holding the operator's files.
  void openDirectoryRequested(const OperatorDescription& op);

private:
  Q_DISABLE_COPY(CustomOperatorManagerDialog)

  void addSection(QGridLayout* grid, int& row, const QString& title);
  void addOperator(QGridLayout* grid, int& row, const OperatorDescription& op);

  Provider m_provider;
  std::vector<OperatorDescription> m_operators;

  QScrollArea* m_scroll = nullptr;
  QLabel* m_emptyHint = nullptr;
};

} // namespace tomviz

#endif

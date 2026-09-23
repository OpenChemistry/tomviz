/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */
#include "CustomOperatorManagerDialog.h"

#include "Utilities.h"

#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace tomviz {

namespace {

enum Column
{
  NameColumn,
  PathColumn,
  StatusColumn,
  EditColumn,
  DeleteColumn,
  CloneColumn,
  OpenColumn,
  ColumnCount
};

/// A label that shows as much of a long path as fits, eliding the middle
/// so both the root and the leaf stay readable; the full path is the
/// tooltip. Its width is decided by the layout, never by the text.
class ElidedPathLabel : public QLabel
{
public:
  ElidedPathLabel(const QString& path, QWidget* parent)
    : QLabel(parent), m_path(path)
  {
    setToolTip(path);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    setStyleSheet("QLabel { color: palette(mid); }");
  }

protected:
  void resizeEvent(QResizeEvent* event) override
  {
    QLabel::resizeEvent(event);
    setText(fontMetrics().elidedText(m_path, Qt::ElideMiddle,
                                     contentsRect().width()));
  }

private:
  QString m_path;
};

} // namespace

CustomOperatorManagerDialog::CustomOperatorManagerDialog(Provider provider,
                                                         QWidget* parent)
  : QDialog(parent), m_provider(std::move(provider))
{
  setWindowTitle(tr("Manage Custom Transforms"));
  // Like the node editor: float above the main window rather than slip
  // behind it while the user consults the pipeline.
  floatAboveMainWindow(this);

  auto* layout = new QVBoxLayout(this);

  m_scroll = new QScrollArea(this);
  m_scroll->setObjectName(QStringLiteral("customOperatorList"));
  m_scroll->setWidgetResizable(true);
  m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  layout->addWidget(m_scroll, 1);

  m_emptyHint = new QLabel(
    tr("No custom transforms yet. Create a new one, or turn a node into "
       "one from its editor with \"Save as Custom Transform\"."),
    this);
  m_emptyHint->setObjectName(QStringLiteral("customOperatorEmptyHint"));
  m_emptyHint->setWordWrap(true);
  m_emptyHint->setStyleSheet("QLabel { color: palette(mid); }");
  layout->addWidget(m_emptyHint);

  auto* bottom = new QHBoxLayout;
  layout->addLayout(bottom);
  auto* createButton = new QPushButton(tr("Create New..."), this);
  createButton->setObjectName(QStringLiteral("createCustomOperator"));
  connect(createButton, &QPushButton::clicked, this,
          &CustomOperatorManagerDialog::createRequested);
  bottom->addWidget(createButton);
  auto* refreshButton = new QPushButton(tr("Refresh"), this);
  refreshButton->setObjectName(QStringLiteral("refreshCustomOperators"));
  refreshButton->setToolTip(
    tr("Rescan the directories, for changes made outside tomviz"));
  connect(refreshButton, &QPushButton::clicked, this,
          &CustomOperatorManagerDialog::refresh);
  bottom->addWidget(refreshButton);
  bottom->addStretch();
  auto* closeButton = new QPushButton(tr("Close"), this);
  closeButton->setDefault(true);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
  bottom->addWidget(closeButton);

  resize(760, 420);
  refresh();
}

void CustomOperatorManagerDialog::refresh()
{
  m_operators = m_provider ? m_provider() : std::vector<OperatorDescription>();

  // Rebuilt from scratch each time; the scroll area deletes the old one.
  auto* content = new QWidget(m_scroll);
  auto* grid = new QGridLayout(content);
  grid->setColumnStretch(PathColumn, 1);
  grid->setHorizontalSpacing(12);

  int row = 0;
  for (auto type : { OperatorDescription::Type::Source,
                     OperatorDescription::Type::Transform }) {
    // Legacy transforms are transforms as far as the user is concerned.
    const bool sources = type == OperatorDescription::Type::Source;
    bool any = false;
    for (const auto& op : m_operators) {
      if ((op.type == OperatorDescription::Type::Source) != sources) {
        continue;
      }
      if (!any) {
        addSection(grid, row, sources ? tr("Sources") : tr("Transforms"));
        any = true;
      }
      addOperator(grid, row, op);
    }
  }
  grid->setRowStretch(row, 1);
  m_scroll->setWidget(content);

  m_emptyHint->setVisible(m_operators.empty());
}

void CustomOperatorManagerDialog::addSection(QGridLayout* grid, int& row,
                                             const QString& title)
{
  auto* header = new QLabel(title, grid->parentWidget());
  header->setObjectName(QStringLiteral("customOperatorSection"));
  header->setStyleSheet("QLabel { font-weight: bold; }");
  if (row > 0) {
    header->setContentsMargins(0, 12, 0, 0);
  }
  grid->addWidget(header, row++, 0, 1, ColumnCount);
}

void CustomOperatorManagerDialog::addOperator(QGridLayout* grid, int& row,
                                              const OperatorDescription& op)
{
  QWidget* parent = grid->parentWidget();
  // Every widget of the row carries the label, so a test (or a handler)
  // can tell the rows apart without a container widget per row.
  auto tag = [&op](QWidget* widget) {
    widget->setProperty("operatorLabel", op.label);
  };

  auto* name = new QLabel(op.label, parent);
  name->setObjectName(QStringLiteral("customOperatorName"));
  name->setToolTip(QDir::toNativeSeparators(op.pythonPath));
  name->setEnabled(op.valid);
  tag(name);
  grid->addWidget(name, row, NameColumn);

  const QString directory =
    QDir::toNativeSeparators(QFileInfo(op.pythonPath).absolutePath());
  auto* path = new ElidedPathLabel(directory, parent);
  path->setObjectName(QStringLiteral("customOperatorPath"));
  tag(path);
  grid->addWidget(path, row, PathColumn);

  if (!op.valid) {
    auto* status = new QLabel(tr("broken"), parent);
    status->setObjectName(QStringLiteral("customOperatorStatus"));
    status->setStyleSheet("QLabel { color: palette(mid); }");
    status->setToolTip(op.loadError.isEmpty()
                         ? tr("The file does not contain a valid operator "
                              "definition.")
                         : op.loadError);
    tag(status);
    grid->addWidget(status, row, StatusColumn);
  }

  // Same rule as everywhere: only the user's own files are touched in
  // place; anything else is copied into the user's directory first.
  const QString whyNot =
    op.userOwned ? QString()
                 : tr("Only nodes in your tomviz directory can be changed. "
                      "Clone this one to get your own copy.");
  // Icon buttons with the verb as tooltip; a disabled one explains
  // itself instead. A missing icon resource falls back to the verb as
  // text rather than a blank button.
  const int iconSize =
    parent->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, parent);
  auto makeButton = [&](const QString& iconResource, const QString& text,
                        const char* objectName, int column, bool enabled) {
    auto* button = new QToolButton(parent);
    button->setObjectName(QString::fromLatin1(objectName));
    button->setAutoRaise(true);
    button->setEnabled(enabled);
    button->setToolTip(enabled ? text : whyNot);
    QIcon icon(iconResource);
    if (icon.pixmap(QSize(iconSize, iconSize)).isNull()) {
      button->setText(text);
      button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    } else {
      button->setIcon(icon);
      button->setIconSize(QSize(iconSize, iconSize));
    }
    tag(button);
    grid->addWidget(button, row, column);
    return button;
  };
  auto* edit = makeButton(QStringLiteral(":/icons/edit.png"), tr("Edit"),
                          "editCustomOperator", EditColumn, op.userOwned);
  auto* remove =
    makeButton(QStringLiteral(":/QtWidgets/Icons/pqDelete.svg"), tr("Delete"),
               "deleteCustomOperator", DeleteColumn, op.userOwned);
  auto* clone = makeButton(QStringLiteral(":/icons/clone.svg"), tr("Clone"),
                           "cloneCustomOperator", CloneColumn, true);
  auto* open =
    makeButton(QStringLiteral(":/icons/folder.svg"),
               tr("Open containing folder"), "openCustomOperatorDirectory",
               OpenColumn, true);

  // A handler may refresh this dialog, which destroys these buttons and
  // with them the closures below, so each hands out a copy that lives on
  // its own stack frame for the duration of the emit.
  connect(edit, &QToolButton::clicked, this, [this, op]() {
    const OperatorDescription copy = op;
    emit editRequested(copy);
  });
  connect(remove, &QToolButton::clicked, this, [this, op]() {
    const OperatorDescription copy = op;
    emit deleteRequested(copy);
  });
  connect(clone, &QToolButton::clicked, this, [this, op]() {
    const OperatorDescription copy = op;
    emit cloneRequested(copy);
  });
  connect(open, &QToolButton::clicked, this, [this, op]() {
    const OperatorDescription copy = op;
    emit openDirectoryRequested(copy);
  });

  ++row;
}

} // namespace tomviz

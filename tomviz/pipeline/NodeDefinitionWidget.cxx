/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeDefinitionWidget.h"

#include "NodeDefinitionFormWidget.h"

#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStackedWidget>
#include <QSyntaxHighlighter>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace {

class JsonSyntaxHighlighter : public QSyntaxHighlighter
{
public:
  explicit JsonSyntaxHighlighter(QTextDocument* document)
    : QSyntaxHighlighter(document)
  {
    m_key.setForeground(QColor(0x8b, 0x4f, 0xbb));
    m_string.setForeground(QColor(0x2e, 0x8b, 0x57));
    m_number.setForeground(QColor(0x1e, 0x6f, 0xba));
    m_literal.setForeground(QColor(0xb2, 0x5c, 0x00));
  }

protected:
  void highlightBlock(const QString& text) override
  {
    // Keys first, then remaining strings, so `"a": "b"` colors both
    // halves differently.
    static const QRegularExpression key(
      QStringLiteral("\"(?:[^\"\\\\]|\\\\.)*\"\\s*(?=:)"));
    static const QRegularExpression string(
      QStringLiteral("\"(?:[^\"\\\\]|\\\\.)*\""));
    static const QRegularExpression number(
      QStringLiteral("-?\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b"));
    static const QRegularExpression literal(
      QStringLiteral("\\b(?:true|false|null)\\b"));

    applyRule(text, number, m_number);
    applyRule(text, literal, m_literal);
    applyRule(text, string, m_string);
    applyRule(text, key, m_key);
  }

private:
  void applyRule(const QString& text, const QRegularExpression& expression,
                 const QTextCharFormat& format)
  {
    auto it = expression.globalMatch(text);
    while (it.hasNext()) {
      auto match = it.next();
      setFormat(match.capturedStart(), match.capturedLength(), format);
    }
  }

  QTextCharFormat m_key;
  QTextCharFormat m_string;
  QTextCharFormat m_number;
  QTextCharFormat m_literal;
};

} // namespace

namespace tomviz {
namespace pipeline {

NodeDefinitionWidget::NodeDefinitionWidget(const QString& json,
                                           NodeShape shape,
                                           DefinitionSchema schema,
                                           QWidget* parent)
  : NodeDefinitionWidget(json, shape, schema, DefinitionTarget::LiveNode,
                         parent)
{
}

NodeDefinitionWidget::NodeDefinitionWidget(const QString& json,
                                           NodeShape shape,
                                           DefinitionSchema schema,
                                           DefinitionTarget target,
                                           QWidget* parent)
  : QWidget(parent), m_shape(shape), m_schema(schema), m_target(target),
    m_appliedJson(json)
{
  m_renderedParameters = QJsonDocument::fromJson(json.toUtf8())
                           .object()
                           .value(QStringLiteral("parameters"))
                           .toArray();

  auto* layout = new QVBoxLayout(this);

  auto* headerRow = new QHBoxLayout;
  auto* header = new QLabel(
    m_target == DefinitionTarget::File
      ? tr("Edits are written to the definition file when you save. Nodes "
           "already in a pipeline keep the description they were created "
           "with.")
      : tr("Edits apply to this node only, and are saved with the state "
           "file. Ports and the node's schema are fixed once it exists."),
    this);
  header->setWordWrap(true);
  header->setStyleSheet("QLabel { color: palette(mid); }");
  headerRow->addWidget(header, 1);

  m_rawButton = new QPushButton(tr("Raw"), this);
  m_rawButton->setCheckable(true);
  m_rawButton->setToolTip(
    tr("Edit the description as raw JSON, including the keys the form "
       "doesn't show"));
  headerRow->addWidget(m_rawButton, 0, Qt::AlignTop);
  layout->addLayout(headerRow);

  m_stack = new QStackedWidget(this);
  layout->addWidget(m_stack, 1);

  m_form = new NodeDefinitionFormWidget(shape, schema, m_target, m_stack);
  m_stack->addWidget(m_form);

  m_editor = new QTextEdit(m_stack);
  m_editor->setObjectName(QStringLiteral("definitionRawEditor"));
  m_editor->setLineWrapMode(QTextEdit::NoWrap);
  m_editor->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_editor->setPlainText(json);
  new JsonSyntaxHighlighter(m_editor->document());
  m_stack->addWidget(m_editor);

  m_issueLabel = new QLabel(this);
  m_issueLabel->setWordWrap(true);
  m_issueLabel->setTextFormat(Qt::RichText);
  m_issueLabel->hide();
  layout->addWidget(m_issueLabel);

  m_debounce = new QTimer(this);
  m_debounce->setSingleShot(true);
  m_debounce->setInterval(400);

  connect(m_editor, &QTextEdit::textChanged, this,
          [this]() { m_debounce->start(); });
  connect(m_debounce, &QTimer::timeout, this,
          &NodeDefinitionWidget::revalidate);
  connect(m_form, &NodeDefinitionFormWidget::changed, this,
          &NodeDefinitionWidget::onFormChanged);
  connect(m_rawButton, &QPushButton::toggled, this,
          &NodeDefinitionWidget::setRawMode);

  // The form is the default; raw is the escape hatch for the keys it
  // doesn't render.
  QSettings settings;
  bool wantRaw =
    settings.value(QStringLiteral("pipeline/nodeDefinitionRawMode"), false)
      .toBool();
  // setChecked fires the handler only on an actual change, so silence it
  // and drive the initial state once — letting both run would populate
  // the form twice and leave a set of orphaned rows on screen.
  m_rawButton->blockSignals(true);
  m_rawButton->setChecked(wantRaw);
  m_rawButton->blockSignals(false);
  setRawMode(wantRaw);

  revalidate();
}

void NodeDefinitionWidget::setRawMode(bool raw)
{
  if (!raw) {
    // The form is built from a parsed document, so it simply cannot
    // represent text that doesn't parse. Stay in raw rather than
    // silently discarding whatever the user was part-way through.
    m_syncing = true;
    bool loaded = m_form->setJson(definitionText());
    m_syncing = false;
    if (!loaded) {
      m_rawButton->setChecked(true);
      m_issueLabel->setText(
        tr("<b>Fix the JSON before leaving Raw — the form can only show a "
           "document that parses.</b>"));
      m_issueLabel->show();
      return;
    }
  }
  m_stack->setCurrentWidget(raw ? static_cast<QWidget*>(m_editor)
                                : static_cast<QWidget*>(m_form));
  QSettings().setValue(QStringLiteral("pipeline/nodeDefinitionRawMode"), raw);
}

void NodeDefinitionWidget::onFormChanged()
{
  if (m_syncing) {
    return;
  }
  // The text buffer stays authoritative: writing the form's document
  // into it is what kicks off validation and the Parameters-tab
  // re-render, so both views go through exactly one code path.
  m_syncing = true;
  m_editor->setPlainText(m_form->json());
  m_syncing = false;
}

QString NodeDefinitionWidget::definitionText() const
{
  return m_editor->toPlainText();
}

bool NodeDefinitionWidget::isValid() const
{
  return m_valid;
}

void NodeDefinitionWidget::flushPendingValidation()
{
  if (!m_debounce->isActive()) {
    return;
  }
  m_debounce->stop();
  revalidate();
}

void NodeDefinitionWidget::markApplied(const QString& json)
{
  m_appliedJson = json;
  // The baseline moved, so warnings phrased against the old description
  // ("dropping foo") are stale even though the text didn't change.
  revalidate();
}

void NodeDefinitionWidget::revalidate()
{
  const QString text = definitionText();
  auto validation = validateNodeDefinition(m_appliedJson, text, m_shape,
                                           m_schema, m_target);

  renderIssues(validation);

  bool valid = !validation.hasErrors();
  if (valid != m_valid) {
    m_valid = valid;
    emit validityChanged(m_valid);
  }
  if (!valid) {
    return;
  }

  auto parameters = QJsonDocument::fromJson(text.toUtf8())
                      .object()
                      .value(QStringLiteral("parameters"))
                      .toArray();
  if (parameters != m_renderedParameters) {
    m_renderedParameters = parameters;
    emit parameterSchemaChanged(text);
  }
}

void NodeDefinitionWidget::renderIssues(const DefinitionValidation& validation)
{
  QStringList lines;
  for (const auto& message :
       validation.messages(DefinitionIssue::Severity::Error)) {
    lines.append(QStringLiteral("<b>%1</b>").arg(message.toHtmlEscaped()));
  }
  for (const auto& message :
       validation.messages(DefinitionIssue::Severity::Warning)) {
    lines.append(message.toHtmlEscaped());
  }

  if (lines.isEmpty()) {
    m_issueLabel->clear();
    m_issueLabel->hide();
    return;
  }

  bool errors = validation.hasErrors();
  m_issueLabel->setStyleSheet(
    errors ? "QLabel { color: palette(bright-text); background: #8b1a1a; "
             "border-radius: 3px; padding: 5px; }"
           : "QLabel { color: palette(text); background: palette(alternate-"
             "base); border-radius: 3px; padding: 5px; }");
  m_issueLabel->setText(lines.join(QStringLiteral("<br>")));
  m_issueLabel->show();
}

} // namespace pipeline
} // namespace tomviz

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "NodeDefinitionFormWidget.h"

#include "NodeDefinitionEdits.h"
#include "PortType.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

const char* kParametersKey = "parameters";
const char* kInputsKey = "inputs";
const char* kOutputsKey = "outputs";

const QStringList& parameterTypeNames()
{
  static const QStringList types = {
    QStringLiteral("double"),         QStringLiteral("int"),
    QStringLiteral("bool"),           QStringLiteral("string"),
    QStringLiteral("enumeration"),    QStringLiteral("file"),
    QStringLiteral("save_file"),      QStringLiteral("directory"),
    QStringLiteral("select_scalars"), QStringLiteral("xyz_header"),
    QStringLiteral("dataset")
  };
  return types;
}

/// Port types the pipeline actually implements. Image, Scalar and Array
/// exist in the PortType enum but nothing produces or consumes them, so
/// offering them here would only invite descriptions that can't run.
const QList<tomviz::pipeline::PortType>& offeredPortTypes()
{
  using tomviz::pipeline::PortType;
  static const QList<PortType> types = {
    PortType::ImageData, PortType::TiltSeries, PortType::Volume,
    PortType::LabelMap,  PortType::Table,      PortType::Molecule
  };
  return types;
}

/// Ports can't be edited yet: validateNodeDefinition rejects every port
/// change, because nothing in the pipeline can remove a port or the links
/// hanging off one. Until that reconciliation lands the sections are
/// hidden rather than shown greyed — a disabled editor still advertises
/// an edit that could never be applied. Flip this with the backend work.
constexpr bool kPortsEditable = false;

bool isStringType(const QString& type)
{
  return type == QLatin1String("string") || type == QLatin1String("file") ||
         type == QLatin1String("save_file") ||
         type == QLatin1String("directory");
}

bool isNumericType(const QString& type)
{
  return type == QLatin1String("double") || type == QLatin1String("int");
}

/// Types whose value the script actually receives. The rest are layout
/// markers (xyz_header), pickers driven by live data (select_scalars),
/// or ports in disguise (dataset) — writing a "default" for those just
/// litters the description with a meaningless 0.
bool hasDefaultValue(const QString& type)
{
  return isNumericType(type) || isStringType(type) ||
         type == QLatin1String("bool") ||
         type == QLatin1String("enumeration");
}

/// A titled section with no frame of its own. The tab is already a
/// container; boxing each section inside it just stacks up chrome.
/// Returns the container (hide it to hide the whole section) and writes
/// the layout its content goes into to @a contentLayout.
QWidget* addSection(QVBoxLayout* parentLayout, const QString& title,
                    QWidget* owner, QVBoxLayout*& contentLayout)
{
  auto* container = new QWidget(owner);
  auto* outer = new QVBoxLayout(container);
  outer->setContentsMargins(0, 8, 0, 0);
  outer->setSpacing(4);

  auto* header = new QLabel(title, container);
  header->setStyleSheet("QLabel { font-weight: bold; }");
  outer->addWidget(header);

  parentLayout->addWidget(container);
  contentLayout = outer;
  return container;
}

/// Parse @a text as JSON, falling back to treating it as a plain string
/// so a user typing `nearest` gets `"nearest"` rather than nothing.
QJsonValue parseOrString(const QString& text)
{
  if (text.trimmed().isEmpty()) {
    return QJsonValue(QJsonValue::Undefined);
  }
  QJsonValue value = tomviz::pipeline::parseJsonValue(text);
  return value.isUndefined() ? QJsonValue(text) : value;
}

/// Checkable pencil: picks which parameter the detail pane below is
/// editing and stays depressed while it is open, so clicking it again
/// closes the pane.
QPushButton* makeEditButton(QWidget* parent)
{
  auto* button = new QPushButton(QStringLiteral("✎"), parent);
  button->setCheckable(true);
  button->setFixedWidth(28);
  button->setToolTip(QObject::tr("Show or hide this parameter's options"));
  return button;
}

QPushButton* makeRemoveButton(QWidget* parent)
{
  auto* button = new QPushButton(QStringLiteral("✕"), parent);
  button->setFixedWidth(28);
  button->setToolTip(QObject::tr("Remove"));
  return button;
}

} // namespace

namespace tomviz {
namespace pipeline {

NodeDefinitionFormWidget::NodeDefinitionFormWidget(NodeShape shape,
                                                   DefinitionSchema schema,
                                                   QWidget* parent)
  : NodeDefinitionFormWidget(shape, schema, DefinitionTarget::LiveNode,
                             parent)
{
}

NodeDefinitionFormWidget::NodeDefinitionFormWidget(NodeShape shape,
                                                   DefinitionSchema schema,
                                                   DefinitionTarget target,
                                                   QWidget* parent)
  : QWidget(parent), m_shape(shape), m_schema(schema), m_target(target)
{
  auto* outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);

  // The form has to scroll: a node with a dozen parameters plus an open
  // detail pane is taller than the properties dock.
  auto* scroll = new QScrollArea(this);
  scroll->setObjectName(QStringLiteral("nodeDefinitionScroll"));
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  outer->addWidget(scroll);

  auto* content = new QWidget(scroll);
  content->setObjectName(QStringLiteral("nodeDefinitionScrollContainer"));
  auto* layout = new QVBoxLayout(content);
  scroll->setWidget(content);
  // setWidget() turns autoFillBackground back on, so this has to follow
  // it rather than precede it.
  content->setAutoFillBackground(false);
  scroll->viewport()->setAutoFillBackground(false);
  // ...and the flags alone aren't enough: a stylesheet anywhere up the
  // tree (MainWindow sets one) switches this subtree to QStyleSheetStyle,
  // which paints scroll-area backgrounds from the style instead of
  // honouring those flags. Naming the pieces and declaring them
  // transparent is the same escape hatch MainWindow itself uses to keep
  // #pipelineScroll white — see the comment at MainWindow.cxx:196.
  scroll->setStyleSheet(
    QStringLiteral("#nodeDefinitionScroll, #nodeDefinitionScroll > QWidget,"
                   " #nodeDefinitionScrollContainer"
                   " { background: transparent; }"));

  buildGeneralSection(layout);
  buildPortsSections(layout);
  buildParametersSection(layout);
  buildBehaviorSection(layout);
  layout->addStretch();
}

void NodeDefinitionFormWidget::buildGeneralSection(QVBoxLayout* layout)
{
  QVBoxLayout* content = nullptr;
  auto* box = addSection(layout, tr("General"), this, content);
  auto* form = new QFormLayout;
  form->setContentsMargins(0, 0, 0, 0);
  content->addLayout(form);

  m_nameEdit = new QLineEdit(box);
  m_nameEdit->setToolTip(
    tr("Identifier used for the generated Python module, so it is what "
       "shows up in tracebacks."));
  form->addRow(tr("Name"), m_nameEdit);

  m_labelEdit = new QLineEdit(box);
  m_labelEdit->setToolTip(tr("Menu and node-card text."));
  form->addRow(tr("Label"), m_labelEdit);

  m_descriptionEdit = new QLineEdit(box);
  form->addRow(tr("Description"), m_descriptionEdit);

  if (identityEditable()) {
    // A definition file is bound to no node class, so the schema a live
    // node freezes is a plain field here. The custom widget key is not:
    // anyone who knows what to put there can type it in Raw.
    m_schemaCombo = new QComboBox(box);
    m_schemaCombo->setObjectName(QStringLiteral("definitionSchemaCombo"));
    m_schemaCombo->addItem(tr("Legacy (v1)"), 1);
    m_schemaCombo->addItem(tr("v2"), 2);
    m_schemaCombo->setToolTip(
      tr("v2 declares its ports under \"inputs\" and \"outputs\"; the "
         "legacy schema takes them from \"inputType\", \"outputType\", "
         "\"results\", \"children\" and \"dataset\" parameters."));
    form->addRow(tr("Schema"), m_schemaCombo);
    connect(m_schemaCombo, &QComboBox::currentIndexChanged, this,
            [this]() { commitSchema(); });
  } else {
    m_fixedInfoLabel = new QLabel(box);
    m_fixedInfoLabel->setStyleSheet("QLabel { color: palette(mid); }");
    m_fixedInfoLabel->setWordWrap(true);
    form->addRow(m_fixedInfoLabel);
  }

  for (auto* edit : { m_nameEdit, m_labelEdit, m_descriptionEdit }) {
    connect(edit, &QLineEdit::textEdited, this, [this]() { commitRoot(); });
  }
}

void NodeDefinitionFormWidget::buildPortsSections(QVBoxLayout* layout)
{
  auto build = [this, layout](bool input) -> QWidget* {
    QVBoxLayout* content = nullptr;
    auto* box = addSection(
      layout, input ? tr("Input Ports") : tr("Output Ports"), this, content);

    auto* rows = new QVBoxLayout;
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(2);
    content->addLayout(rows);
    (input ? m_inputRowsLayout : m_outputRowsLayout) = rows;

    auto* buttonRow = new QHBoxLayout;
    auto* addButton =
      new QPushButton(input ? tr("Add Input") : tr("Add Output"), box);
    addButton->setObjectName(input ? QStringLiteral("addInputPortButton")
                                   : QStringLiteral("addOutputPortButton"));
    connect(addButton, &QPushButton::clicked, this,
            [this, input]() { addPort(input); });
    buttonRow->addWidget(addButton);
    buttonRow->addStretch();
    content->addLayout(buttonRow);

    if (!identityEditable()) {
      box->setToolTip(
        tr("Ports are fixed for the life of a node, because links in the "
           "pipeline hang off them. Create a new node to change them."));
    }

    return box;
  };

  m_inputBox = build(true);
  // Whether a v2 description is a source or a transform is decided by
  // its inputs alone, so the note sits right under them.
  m_shapeNote = new QLabel(m_inputBox);
  m_shapeNote->setObjectName(QStringLiteral("definitionShapeNote"));
  m_shapeNote->setWordWrap(true);
  m_shapeNote->setStyleSheet("QLabel { color: palette(mid); }");
  m_inputBox->layout()->addWidget(m_shapeNote);
  m_outputBox = build(false);

  // Legacy schema: the two port keys the form renders as type pickers,
  // and a pointer to raw mode for the rest.
  QVBoxLayout* legacyContent = nullptr;
  m_legacyPortsBox =
    addSection(layout, tr("Ports (legacy schema)"), this, legacyContent);
  auto* legacyForm = new QFormLayout;
  legacyForm->setContentsMargins(0, 0, 0, 0);
  legacyContent->addLayout(legacyForm);
  auto makeTypeCombo = [this](const char* objectName) {
    auto* combo = new QComboBox(m_legacyPortsBox);
    combo->setObjectName(QString::fromLatin1(objectName));
    combo->addItem(tr("(unset)"), QString());
    // Legacy operators only ever take and return volumes; tables and
    // molecules travel as "results" or "children" instead.
    for (auto type : offeredPortTypes()) {
      if (isVolumeType(type)) {
        combo->addItem(portTypeToString(type), portTypeToString(type));
      }
    }
    connect(combo, &QComboBox::currentIndexChanged, this,
            [this]() { commitLegacyPorts(); });
    return combo;
  };
  m_legacyInputType = makeTypeCombo("legacyInputTypeCombo");
  legacyForm->addRow(tr("Input type"), m_legacyInputType);
  m_legacyOutputType = makeTypeCombo("legacyOutputTypeCombo");
  legacyForm->addRow(tr("Output type"), m_legacyOutputType);

  m_legacyPortsNote = new QLabel(m_legacyPortsBox);
  m_legacyPortsNote->setWordWrap(true);
  m_legacyPortsNote->setStyleSheet("QLabel { color: palette(mid); }");
  m_legacyPortsNote->setText(
    tr("Extra result ports (\"results\", \"children\") and \"dataset\" "
       "parameters have no controls here; edit them in Raw."));
  legacyContent->addWidget(m_legacyPortsNote);
}

void NodeDefinitionFormWidget::buildParametersSection(QVBoxLayout* layout)
{
  QVBoxLayout* boxLayout = nullptr;
  auto* box = addSection(layout, tr("Parameters"), this, boxLayout);

  auto* header = new QHBoxLayout;
  auto* nameHeader = new QLabel(tr("Name"), box);
  auto* labelHeader = new QLabel(tr("Label"), box);
  auto* typeHeader = new QLabel(tr("Type"), box);
  for (auto* label : { nameHeader, labelHeader, typeHeader }) {
    label->setStyleSheet("QLabel { color: palette(mid); }");
  }
  header->addWidget(nameHeader, 3);
  header->addWidget(labelHeader, 3);
  header->addWidget(typeHeader, 2);
  header->addSpacing(34);
  boxLayout->addLayout(header);

  m_paramRowsLayout = new QVBoxLayout;
  m_paramRowsLayout->setContentsMargins(0, 0, 0, 0);
  m_paramRowsLayout->setSpacing(2);
  boxLayout->addLayout(m_paramRowsLayout);

  auto* buttonRow = new QHBoxLayout;
  auto* addButton = new QPushButton(tr("Add Parameter"), box);
  connect(addButton, &QPushButton::clicked, this,
          &NodeDefinitionFormWidget::addParameter);
  buttonRow->addWidget(addButton);
  buttonRow->addStretch();
  boxLayout->addLayout(buttonRow);

  // --- detail pane, following the selected row ---
  m_detail = new QWidget(box);
  auto* form = new QFormLayout(m_detail);
  form->setContentsMargins(0, 8, 0, 0);

  m_detailHeader = new QLabel(m_detail);
  m_detailHeader->setStyleSheet("QLabel { font-weight: bold; }");
  form->addRow(m_detailHeader);

  m_pDescription = new QLineEdit(m_detail);
  form->addRow(tr("Description"), m_pDescription);
  m_pDefault = new QLineEdit(m_detail);
  m_pDefault->setToolTip(
    tr("A JSON value: 1.0, true, \"nearest\", or [128, 128, 128] for the "
       "xyz-triple parameters."));
  form->addRow(tr("Default"), m_pDefault);

  m_pMinimum = new QLineEdit(m_detail);
  form->addRow(tr("Minimum"), m_pMinimum);
  m_pMaximum = new QLineEdit(m_detail);
  form->addRow(tr("Maximum"), m_pMaximum);
  m_pStep = new QLineEdit(m_detail);
  form->addRow(tr("Step"), m_pStep);
  m_pPrecision = new QSpinBox(m_detail);
  m_pPrecision->setRange(-1, 15);
  m_pPrecision->setSpecialValueText(tr("unset"));
  form->addRow(tr("Precision"), m_pPrecision);
  m_numericRows = { m_pMinimum, m_pMaximum, m_pStep };

  // Laid out like the port rows: one line per option, each with its own
  // remove button, and a single Add below.
  m_optionsRow = new QWidget(m_detail);
  auto* optionsLayout = new QVBoxLayout(m_optionsRow);
  optionsLayout->setContentsMargins(0, 0, 0, 0);
  optionsLayout->setSpacing(2);
  m_optionRowsLayout = new QVBoxLayout;
  m_optionRowsLayout->setContentsMargins(0, 0, 0, 0);
  m_optionRowsLayout->setSpacing(2);
  optionsLayout->addLayout(m_optionRowsLayout);
  auto* optionButtonRow = new QHBoxLayout;
  auto* addOptionButton = new QPushButton(tr("Add Option"), m_optionsRow);
  connect(addOptionButton, &QPushButton::clicked, this,
          &NodeDefinitionFormWidget::addOption);
  optionButtonRow->addWidget(addOptionButton);
  optionButtonRow->addStretch();
  optionsLayout->addLayout(optionButtonRow);
  form->addRow(tr("Options"), m_optionsRow);

  boxLayout->addWidget(m_detail);

  for (auto* edit : { m_pDescription, m_pDefault, m_pMinimum, m_pMaximum,
                      m_pStep }) {
    connect(edit, &QLineEdit::textEdited, this,
            [this]() { commitParameterDetail(); });
  }
  connect(m_pPrecision, &QSpinBox::valueChanged, this, [this]() {
    if (!m_populating) {
      commitParameterDetail();
    }
  });
}

void NodeDefinitionFormWidget::buildBehaviorSection(QVBoxLayout* layout)
{
  QVBoxLayout* boxLayout = nullptr;
  auto* box = addSection(layout, tr("Behavior"), this, boxLayout);

  m_cancelCheck = new QCheckBox(tr("Supports cancelling mid-execution"), box);
  m_completeCheck = new QCheckBox(tr("Supports early completion"), box);
  m_externalOnlyCheck =
    new QCheckBox(tr("Requires an external Python environment"), box);
  m_externalOnlyCheck->setToolTip(
    tr("Disables the Internal executor choice on the Execution tab."));

  for (auto* check : { m_cancelCheck, m_completeCheck, m_externalOnlyCheck }) {
    boxLayout->addWidget(check);
    connect(check, &QCheckBox::toggled, this, [this]() {
      if (!m_populating) {
        commitRoot();
      }
    });
  }
}

// --- population ------------------------------------------------------

bool NodeDefinitionFormWidget::setJson(const QString& json)
{
  QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
  if (!doc.isObject() && !json.trimmed().isEmpty()) {
    return false;
  }

  m_populating = true;
  m_root = doc.object();

  m_nameEdit->setText(m_root.value(QStringLiteral("name")).toString());
  m_labelEdit->setText(m_root.value(QStringLiteral("label")).toString());
  m_descriptionEdit->setText(
    m_root.value(QStringLiteral("description")).toString());
  for (auto* edit : { m_nameEdit, m_labelEdit, m_descriptionEdit }) {
    edit->setCursorPosition(0);
  }

  const QString widgetId = m_root.value(QStringLiteral("widget")).toString();
  if (identityEditable()) {
    m_schemaCombo->setCurrentIndex(m_schemaCombo->findData(
      currentSchema() == DefinitionSchema::V2 ? 2 : 1));
  } else {
    QStringList fixed;
    fixed.append(
      tr("Schema v%1").arg(m_schema == DefinitionSchema::V2 ? 2 : 1));
    fixed.append(m_shape == NodeShape::Source ? tr("source node")
                                              : tr("transform node"));
    if (!widgetId.isEmpty()) {
      fixed.append(tr("custom widget \"%1\"").arg(widgetId));
    }
    m_fixedInfoLabel->setText(tr("%1 — fixed for the life of this node.")
                                .arg(fixed.join(QStringLiteral(", "))));
  }

  m_cancelCheck->setChecked(
    m_root.value(QStringLiteral("supportsCancel")).toBool(false));
  m_completeCheck->setChecked(
    m_root.value(QStringLiteral("supportsComplete")).toBool(false));
  m_externalOnlyCheck->setChecked(
    m_root.value(QStringLiteral("externalOnly")).toBool(false));

  refreshSchemaDependentControls();
  refreshParameterRows();
  m_populating = false;

  selectParameter(-1);
  return true;
}

QString NodeDefinitionFormWidget::json() const
{
  return QString::fromUtf8(QJsonDocument(m_root).toJson());
}

QJsonArray NodeDefinitionFormWidget::parameters() const
{
  return m_root.value(QLatin1String(kParametersKey)).toArray();
}

void NodeDefinitionFormWidget::setParameters(const QJsonArray& parameters)
{
  m_root[QLatin1String(kParametersKey)] = parameters;
}

// --- ports -----------------------------------------------------------

NodeDefinitionFormWidget::PortRow NodeDefinitionFormWidget::makePortRow(
  bool input, const QJsonObject& port)
{
  PortRow row;
  row.container = new QWidget(this);
  row.container->setObjectName(input ? QStringLiteral("inputPortRow")
                                     : QStringLiteral("outputPortRow"));
  auto* layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 0, 0, 0);

  row.name = new QLineEdit(port.value(QStringLiteral("name")).toString(),
                           row.container);
  row.name->setPlaceholderText(tr("name"));
  layout->addWidget(row.name, 3);

  row.type = new QComboBox(row.container);
  for (auto type : offeredPortTypes()) {
    row.type->addItem(portTypeToString(type));
  }
  const QString portType = port.value(QStringLiteral("type")).toString();
  int typeIndex = row.type->findText(portType);
  if (typeIndex < 0 && !portType.isEmpty()) {
    // A type this build doesn't offer (or a typo): keep showing it
    // rather than silently rewriting the port to something else.
    row.type->addItem(portType);
    typeIndex = row.type->count() - 1;
  }
  row.type->setCurrentIndex(typeIndex < 0 ? 0 : typeIndex);
  layout->addWidget(row.type, 2);

  if (!input) {
    row.persistence = new QComboBox(row.container);
    row.persistence->addItem(tr("Transient"), false);
    row.persistence->addItem(tr("Persistent"), true);
    row.persistence->setCurrentIndex(
      port.value(QStringLiteral("persistent")).toBool(false) ? 1 : 0);
    layout->addWidget(row.persistence, 2);
  }

  row.remove = makeRemoveButton(row.container);
  layout->addWidget(row.remove);

  connect(row.name, &QLineEdit::textEdited, this,
          [this, input]() { commitPorts(input); });
  connect(row.type, &QComboBox::currentTextChanged, this,
          [this, input]() { commitPorts(input); });
  if (row.persistence) {
    connect(row.persistence, &QComboBox::currentIndexChanged, this,
            [this, input]() { commitPorts(input); });
  }
  connect(row.remove, &QPushButton::clicked, this,
          [this, input, c = row.container]() {
            auto& rows = input ? m_inputRows : m_outputRows;
            for (int i = 0; i < rows.size(); ++i) {
              if (rows[i].container == c) {
                rows.removeAt(i);
                c->hide();
                c->setParent(nullptr);
                c->deleteLater();
                commitPorts(input);
                break;
              }
            }
          });

  return row;
}

void NodeDefinitionFormWidget::rebuildPortRows(bool input)
{
  auto& rows = input ? m_inputRows : m_outputRows;
  auto* layout = input ? m_inputRowsLayout : m_outputRowsLayout;
  for (const auto& row : rows) {
    layout->removeWidget(row.container);
    // deleteLater() alone leaves the widget parented and visible until
    // the event loop drains, so it paints over the rebuilt rows.
    row.container->hide();
    row.container->setParent(nullptr);
    row.container->deleteLater();
  }
  rows.clear();

  const auto ports =
    m_root.value(QLatin1String(input ? kInputsKey : kOutputsKey)).toArray();
  for (const auto& value : ports) {
    PortRow row = makePortRow(input, value.toObject());
    layout->addWidget(row.container);
    rows.append(row);
  }
}

void NodeDefinitionFormWidget::refreshPortRows()
{
  const bool editable = kPortsEditable || identityEditable();
  const bool v2 = currentSchema() == DefinitionSchema::V2;
  // In file mode the inputs list is what makes a description a source
  // or a transform, so it shows either way; a live node's shape is fixed.
  const bool showInputs =
    editable && v2 &&
    (identityEditable() || m_shape == NodeShape::Transform);
  m_inputBox->setVisible(showInputs);
  m_outputBox->setVisible(editable && v2);
  m_legacyPortsBox->setVisible(editable && !v2);
  if (!editable) {
    return;
  }

  if (v2) {
    rebuildPortRows(true);
    rebuildPortRows(false);
    updateShapeNote();
    return;
  }

  // Selecting the pickers must not commit back into the document.
  const bool wasPopulating = m_populating;
  m_populating = true;
  auto select = [this](QComboBox* combo, const char* key) {
    const QString type = m_root.value(QLatin1String(key)).toString();
    int index = combo->findData(type);
    if (index < 0) {
      // A type this build doesn't offer: show it rather than rewrite it.
      combo->addItem(type, type);
      index = combo->count() - 1;
    }
    combo->setCurrentIndex(index);
  };
  select(m_legacyInputType, "inputType");
  select(m_legacyOutputType, "outputType");
  m_populating = wasPopulating;
}

void NodeDefinitionFormWidget::commitPorts(bool input)
{
  if (m_populating) {
    return;
  }
  const auto& rows = input ? m_inputRows : m_outputRows;
  const QString key = QLatin1String(input ? kInputsKey : kOutputsKey);

  // Mutate each existing entry so per-port keys the form doesn't render
  // survive, and only drop entries whose row is actually gone.
  const auto previous = m_root.value(key).toArray();
  QJsonArray ports;
  for (int i = 0; i < rows.size(); ++i) {
    QJsonObject port =
      i < previous.size() ? previous.at(i).toObject() : QJsonObject();
    port[QStringLiteral("name")] = rows[i].name->text();
    port[QStringLiteral("type")] = rows[i].type->currentText();
    if (rows[i].persistence) {
      port[QStringLiteral("persistent")] =
        rows[i].persistence->currentData().toBool();
    }
    ports.append(port);
  }

  if (ports.isEmpty() && !m_root.contains(key)) {
    return;
  }
  m_root[key] = ports;
  updateShapeNote();
  emit changed();
}

void NodeDefinitionFormWidget::addPort(bool input)
{
  const QString key = QLatin1String(input ? kInputsKey : kOutputsKey);
  auto ports = m_root.value(key).toArray();
  ports.append(QJsonObject{
    { QStringLiteral("name"),
      input ? QStringLiteral("input") : QStringLiteral("output") },
    { QStringLiteral("type"), portTypeToString(PortType::ImageData) } });
  m_root[key] = ports;

  rebuildPortRows(input);
  updateShapeNote();
  emit changed();
}

// --- parameters ------------------------------------------------------

NodeDefinitionFormWidget::ParameterRow
NodeDefinitionFormWidget::makeParameterRow(const QJsonObject& param)
{
  ParameterRow row;
  row.container = new QWidget(this);
  auto* layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(3, 1, 3, 1);

  row.name = new QLineEdit(param.value(QStringLiteral("name")).toString(),
                           row.container);
  row.name->setPlaceholderText(tr("name"));
  layout->addWidget(row.name, 3);

  row.label = new QLineEdit(param.value(QStringLiteral("label")).toString(),
                            row.container);
  row.label->setPlaceholderText(tr("label"));
  layout->addWidget(row.label, 3);

  row.type = new QComboBox(row.container);
  row.type->addItems(parameterTypeNames());
  QString type = param.value(QStringLiteral("type")).toString();
  int typeIndex = row.type->findText(type);
  if (typeIndex < 0 && !type.isEmpty()) {
    // An unrecognized type from a hand-written description: offer it
    // rather than silently rewriting it to something else.
    row.type->addItem(type);
    typeIndex = row.type->count() - 1;
  }
  row.type->setCurrentIndex(typeIndex < 0 ? 0 : typeIndex);
  layout->addWidget(row.type, 2);

  row.edit = makeEditButton(row.container);
  layout->addWidget(row.edit);
  connect(row.edit, &QPushButton::clicked, this, [this, c = row.container]() {
    int index = indexOfParameterRow(c);
    if (index >= 0) {
      // Clicking the pencil of the parameter that's already open closes
      // it again, so the detail pane can be dismissed without having to
      // pick some other parameter.
      selectParameter(index == m_selectedParameter ? -1 : index);
    }
  });

  row.remove = makeRemoveButton(row.container);
  layout->addWidget(row.remove);

  auto commit = [this, c = row.container]() {
    int index = indexOfParameterRow(c);
    if (index >= 0) {
      commitParameterRow(index);
    }
  };
  connect(row.name, &QLineEdit::textEdited, this, commit);
  connect(row.label, &QLineEdit::textEdited, this, commit);
  connect(row.type, &QComboBox::currentTextChanged, this, commit);
  connect(row.remove, &QPushButton::clicked, this, [this, c = row.container]() {
    int index = indexOfParameterRow(c);
    if (index >= 0) {
      removeParameter(index);
    }
  });

  return row;
}

void NodeDefinitionFormWidget::refreshParameterRows()
{
  for (const auto& row : m_paramRows) {
    m_paramRowsLayout->removeWidget(row.container);
    // See rebuildPortRows(): hide and unparent before deleteLater() or
    // the outgoing rows paint over the new ones.
    row.container->hide();
    row.container->setParent(nullptr);
    row.container->deleteLater();
  }
  m_paramRows.clear();

  const auto params = parameters();
  for (const auto& value : params) {
    ParameterRow row = makeParameterRow(value.toObject());
    m_paramRowsLayout->addWidget(row.container);
    m_paramRows.append(row);
  }
}

int NodeDefinitionFormWidget::indexOfParameterRow(
  const QWidget* container) const
{
  for (int i = 0; i < m_paramRows.size(); ++i) {
    if (m_paramRows[i].container == container) {
      return i;
    }
  }
  return -1;
}

void NodeDefinitionFormWidget::selectParameter(int index)
{
  m_selectedParameter = index;
  for (int i = 0; i < m_paramRows.size(); ++i) {
    // Driven rather than toggled: clicking an already-depressed pencil
    // would otherwise pop it back out while its parameter stays open.
    QSignalBlocker blocker(m_paramRows[i].edit);
    m_paramRows[i].edit->setChecked(i == index);
  }
  loadParameterDetail();
}

void NodeDefinitionFormWidget::loadParameterDetail()
{
  const auto params = parameters();
  bool valid = m_selectedParameter >= 0 && m_selectedParameter < params.size();
  m_detail->setVisible(valid);
  if (!valid) {
    return;
  }

  bool wasPopulating = m_populating;
  m_populating = true;

  auto fields = readParameterFields(params.at(m_selectedParameter).toObject());
  m_detailHeader->setText(
    fields.name.isEmpty()
      ? tr("Options for this %1").arg(fields.type)
      : tr("Options for \"%1\"").arg(fields.name));

  m_pDescription->setText(fields.description);
  m_pDefault->setText(
    isStringType(fields.type) && fields.defaultValue.isString()
      ? fields.defaultValue.toString()
      : formatJsonValue(fields.defaultValue));
  m_pMinimum->setText(formatJsonValue(fields.minimum));
  m_pMaximum->setText(formatJsonValue(fields.maximum));
  m_pStep->setText(formatJsonValue(fields.step));
  m_pPrecision->setValue(
    fields.precision.isUndefined() ? -1 : fields.precision.toInt(-1));

  refreshOptionRows(fields.options);

  for (auto* edit :
       { m_pDescription, m_pDefault, m_pMinimum, m_pMaximum, m_pStep }) {
    edit->setCursorPosition(0);
  }

  updateTypeDependentRows();
  m_populating = wasPopulating;
}

void NodeDefinitionFormWidget::updateTypeDependentRows()
{
  auto* form = qobject_cast<QFormLayout*>(m_detail->layout());
  if (!form || m_selectedParameter < 0 ||
      m_selectedParameter >= m_paramRows.size()) {
    return;
  }
  const QString type = m_paramRows[m_selectedParameter].type->currentText();
  for (auto* widget : m_numericRows) {
    form->setRowVisible(widget, isNumericType(type));
  }
  // Decimal places only mean anything for a floating-point control.
  form->setRowVisible(m_pPrecision, type == QLatin1String("double"));
  form->setRowVisible(m_optionsRow, type == QLatin1String("enumeration"));
  form->setRowVisible(m_pDefault, hasDefaultValue(type));
}

void NodeDefinitionFormWidget::commitParameterRow(int index)
{
  if (m_populating) {
    return;
  }
  auto params = parameters();
  if (index < 0 || index >= params.size() || index >= m_paramRows.size()) {
    return;
  }

  QJsonObject param = params.at(index).toObject();
  const QString previousType = param.value(QStringLiteral("type")).toString();
  const QString newType = m_paramRows[index].type->currentText();
  const QString newName = m_paramRows[index].name->text();

  setOrClear(param, QStringLiteral("name"), newName);
  setOrClear(param, QStringLiteral("label"), m_paramRows[index].label->text());
  setOrClear(param, QStringLiteral("type"), newType);
  params.replace(index, param);
  setParameters(params);

  if (index == m_selectedParameter) {
    if (previousType != newType) {
      // The type decides which rows the detail pane offers and which
      // keys survive, so rebuild it and re-commit under the new type.
      loadParameterDetail();
      commitParameterDetail();
    } else {
      // Only the header tracks the name — rebuilding the whole detail
      // pane on every keystroke would tear down the option rows too.
      m_detailHeader->setText(newName.isEmpty()
                                ? tr("Options for this %1").arg(newType)
                                : tr("Options for \"%1\"").arg(newName));
    }
  }
  emit changed();
}

QJsonArray NodeDefinitionFormWidget::collectOptions() const
{
  QJsonArray options;
  for (const auto& row : m_optionRows) {
    const QString label = row.label->text();
    if (label.isEmpty()) {
      continue;
    }
    QJsonValue value = parseOrString(row.value->text());
    if (value.isUndefined()) {
      // An option has to carry a value: writing an undefined one into a
      // QJsonObject removes the key instead, leaving {} behind — which
      // is not a renderable option. A half-typed row means an empty
      // string, not a malformed entry.
      value = QString();
    }
    options.append(QJsonObject{ { label, value } });
  }
  return options;
}

NodeDefinitionFormWidget::OptionRow NodeDefinitionFormWidget::makeOptionRow(
  const QString& label, const QJsonValue& value)
{
  OptionRow row;
  row.container = new QWidget(m_optionsRow);
  auto* layout = new QHBoxLayout(row.container);
  layout->setContentsMargins(0, 0, 0, 0);

  row.label = new QLineEdit(label, row.container);
  row.label->setPlaceholderText(tr("label"));
  layout->addWidget(row.label, 3);

  row.value = new QLineEdit(formatJsonValue(value), row.container);
  row.value->setPlaceholderText(tr("value"));
  row.value->setCursorPosition(0);
  layout->addWidget(row.value, 3);

  row.remove = makeRemoveButton(row.container);
  layout->addWidget(row.remove);

  connect(row.label, &QLineEdit::textEdited, this,
          [this]() { commitParameterDetail(); });
  connect(row.value, &QLineEdit::textEdited, this,
          [this]() { commitParameterDetail(); });
  connect(row.remove, &QPushButton::clicked, this,
          [this, c = row.container]() {
            for (int i = 0; i < m_optionRows.size(); ++i) {
              if (m_optionRows[i].container == c) {
                m_optionRows.removeAt(i);
                c->hide();
                c->setParent(nullptr);
                c->deleteLater();
                commitParameterDetail();
                break;
              }
            }
          });

  return row;
}

void NodeDefinitionFormWidget::refreshOptionRows(const QJsonArray& options)
{
  for (const auto& row : m_optionRows) {
    m_optionRowsLayout->removeWidget(row.container);
    row.container->hide();
    row.container->setParent(nullptr);
    row.container->deleteLater();
  }
  m_optionRows.clear();

  for (const auto& value : options) {
    QJsonObject option = value.toObject();
    if (option.isEmpty()) {
      continue;
    }
    OptionRow row =
      makeOptionRow(option.constBegin().key(), option.constBegin().value());
    m_optionRowsLayout->addWidget(row.container);
    m_optionRows.append(row);
  }
}

void NodeDefinitionFormWidget::addOption()
{
  OptionRow row = makeOptionRow(QString(), QJsonValue(QJsonValue::Undefined));
  m_optionRowsLayout->addWidget(row.container);
  m_optionRows.append(row);
  row.label->setFocus();
  // Not committed yet: an option with a blank label is skipped by
  // collectOptions, so it lands once the user names it.
}

void NodeDefinitionFormWidget::commitParameterDetail()
{
  if (m_populating) {
    return;
  }
  auto params = parameters();
  if (m_selectedParameter < 0 || m_selectedParameter >= params.size()) {
    return;
  }

  QJsonObject param = params.at(m_selectedParameter).toObject();
  auto fields = readParameterFields(param);

  fields.description = m_pDescription->text();
  fields.defaultValue = QJsonValue(QJsonValue::Undefined);
  if (hasDefaultValue(fields.type)) {
    fields.defaultValue =
      isStringType(fields.type)
        ? (m_pDefault->text().isEmpty() ? QJsonValue(QJsonValue::Undefined)
                                        : QJsonValue(m_pDefault->text()))
        : parseOrString(m_pDefault->text());
  }
  fields.minimum = QJsonValue(QJsonValue::Undefined);
  fields.maximum = QJsonValue(QJsonValue::Undefined);
  fields.step = QJsonValue(QJsonValue::Undefined);
  fields.precision = QJsonValue(QJsonValue::Undefined);
  if (isNumericType(fields.type)) {
    fields.minimum = parseJsonValue(m_pMinimum->text());
    fields.maximum = parseJsonValue(m_pMaximum->text());
    fields.step = parseJsonValue(m_pStep->text());
  }
  if (fields.type == QLatin1String("double") && m_pPrecision->value() >= 0) {
    fields.precision = m_pPrecision->value();
  }
  fields.options = fields.type == QLatin1String("enumeration")
                     ? collectOptions()
                     : QJsonArray();
  // visible_if / enable_if aren't rendered; readParameterFields() has
  // already carried them over, so they survive the write untouched.

  // Mutate the existing entry rather than building a fresh one, so keys
  // the form doesn't render survive the edit.
  applyParameterFields(param, fields);
  params.replace(m_selectedParameter, param);
  setParameters(params);
  emit changed();
}

void NodeDefinitionFormWidget::addParameter()
{
  auto params = parameters();

  QString name = QStringLiteral("parameter");
  QStringList taken;
  for (const auto& value : params) {
    taken.append(value.toObject().value(QStringLiteral("name")).toString());
  }
  for (int suffix = 1; taken.contains(name); ++suffix) {
    name = QStringLiteral("parameter_%1").arg(suffix);
  }

  params.append(QJsonObject{
    { QStringLiteral("name"), name },
    { QStringLiteral("label"), name },
    { QStringLiteral("type"), QStringLiteral("double") },
    { QStringLiteral("default"), 0.0 } });
  setParameters(params);

  refreshParameterRows();
  selectParameter(params.size() - 1);
  emit changed();
}

void NodeDefinitionFormWidget::removeParameter(int index)
{
  auto params = parameters();
  if (index < 0 || index >= params.size()) {
    return;
  }
  params.removeAt(index);
  setParameters(params);

  refreshParameterRows();
  selectParameter(
    params.isEmpty() ? -1 : qMin(index, static_cast<int>(params.size()) - 1));
  emit changed();
}

void NodeDefinitionFormWidget::commitRoot()
{
  if (m_populating) {
    return;
  }
  setOrClear(m_root, QStringLiteral("name"), m_nameEdit->text());
  setOrClear(m_root, QStringLiteral("label"), m_labelEdit->text());
  setOrClear(m_root, QStringLiteral("description"),
             m_descriptionEdit->text());
  // "help" isn't rendered, so it is left exactly as the description
  // declared it rather than being cleared on every commit.
  if (currentSchema() == DefinitionSchema::V2) {
    setOrClearBool(m_root, QStringLiteral("supportsCancel"),
                   m_cancelCheck->isChecked(), false);
    setOrClearBool(m_root, QStringLiteral("supportsComplete"),
                   m_completeCheck->isChecked(), false);
  }
  setOrClearBool(m_root, QStringLiteral("externalOnly"),
                 m_externalOnlyCheck->isChecked(), false);
  emit changed();
}

// --- file mode -------------------------------------------------------

bool NodeDefinitionFormWidget::identityEditable() const
{
  return m_target == DefinitionTarget::File;
}

DefinitionSchema NodeDefinitionFormWidget::currentSchema() const
{
  if (!identityEditable()) {
    return m_schema;
  }
  return m_root.value(QStringLiteral("schemaVersion")).toInt(1) == 2
           ? DefinitionSchema::V2
           : DefinitionSchema::V1;
}

NodeShape NodeDefinitionFormWidget::currentShape() const
{
  if (!identityEditable()) {
    return m_shape;
  }
  const bool v2 = currentSchema() == DefinitionSchema::V2;
  return v2 && m_root.value(QLatin1String(kInputsKey)).toArray().isEmpty()
           ? NodeShape::Source
           : NodeShape::Transform;
}

void NodeDefinitionFormWidget::commitSchema()
{
  if (m_populating) {
    return;
  }
  // Shipped v1 descriptors carry no "schemaVersion" at all, so going
  // back to legacy removes the key rather than writing a 1. The port
  // keys of the other schema are left in place for a round trip.
  if (m_schemaCombo->currentData().toInt() == 2) {
    m_root[QStringLiteral("schemaVersion")] = 2;
  } else {
    m_root.remove(QStringLiteral("schemaVersion"));
  }
  refreshSchemaDependentControls();
  emit changed();
}

void NodeDefinitionFormWidget::commitLegacyPorts()
{
  if (m_populating) {
    return;
  }
  setOrClear(m_root, QStringLiteral("inputType"),
             m_legacyInputType->currentData().toString());
  setOrClear(m_root, QStringLiteral("outputType"),
             m_legacyOutputType->currentData().toString());
  emit changed();
}

void NodeDefinitionFormWidget::refreshSchemaDependentControls()
{
  // The legacy schema derives cancel/complete support from the script's
  // operator base class instead, so the checkboxes would be lying.
  const bool v2 = currentSchema() == DefinitionSchema::V2;
  m_cancelCheck->setEnabled(v2);
  m_completeCheck->setEnabled(v2);
  m_cancelCheck->setToolTip(
    v2 ? QString()
       : tr("Legacy nodes take this from the script's operator base "
            "class."));
  m_completeCheck->setToolTip(m_cancelCheck->toolTip());
  refreshPortRows();
}

void NodeDefinitionFormWidget::updateShapeNote()
{
  if (!identityEditable()) {
    m_shapeNote->hide();
    return;
  }
  m_shapeNote->setText(
    currentShape() == NodeShape::Source
      ? tr("No inputs: this description is a source node.")
      : tr("With inputs, this description is a transform node."));
  m_shapeNote->show();
}

} // namespace pipeline
} // namespace tomviz

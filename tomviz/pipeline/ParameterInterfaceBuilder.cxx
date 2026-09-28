/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "ParameterInterfaceBuilder.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include "EnumOptions.h"

#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSet>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <functional>
#include <limits>

namespace {

using tomviz::pipeline::PortScalars;

// --- Templated JSON helpers ---

template <typename T>
bool isType(const QJsonValue&)
{
  return false;
}

template <>
bool isType<int>(const QJsonValue& value)
{
  return value.isDouble();
}

template <>
bool isType<double>(const QJsonValue& value)
{
  return value.isDouble();
}

template <typename T>
T getAs(const QJsonValue&)
{
  return 0;
}

template <>
int getAs(const QJsonValue& value)
{
  return value.toInt();
}

template <>
double getAs(const QJsonValue& value)
{
  return value.toDouble();
}

template <>
QString getAs(const QJsonValue& value)
{
  return value.toString();
}

// --- Numeric widget factory ---

template <typename T>
QWidget* getNumericWidget(T, T, T, int, T)
{
  return nullptr;
}

template <>
QWidget* getNumericWidget(int defaultValue, int rangeMin, int rangeMax,
                          int /*precision*/, int step)
{
  auto* spinBox = new QSpinBox();
  spinBox->setSingleStep(step == -1 ? 1 : step);
  spinBox->setMinimum(rangeMin);
  spinBox->setMaximum(rangeMax);
  spinBox->setValue(defaultValue);
  return spinBox;
}

template <>
QWidget* getNumericWidget(double defaultValue, double rangeMin, double rangeMax,
                          int precision, double step)
{
  auto* spinBox = new QDoubleSpinBox();
  spinBox->setSingleStep(step == -1.0 ? 0.5 : step);
  spinBox->setDecimals(precision != -1 ? precision : 3);
  spinBox->setMinimum(rangeMin);
  spinBox->setMaximum(rangeMax);
  spinBox->setValue(defaultValue);
  return spinBox;
}

// --- Widget type helpers ---

template <typename T>
bool isWidgetType(const QWidget* widget)
{
  return qobject_cast<const T*>(widget) != nullptr;
}

bool isWidgetNumeric(const QWidget* widget)
{
  return isWidgetType<QDoubleSpinBox>(widget) ||
         isWidgetType<QSpinBox>(widget);
}

// --- Widget changed signal ---

template <typename T>
auto changedSignal()
{
  if constexpr (std::is_same_v<T, QComboBox>) {
    return QOverload<int>::of(&QComboBox::currentIndexChanged);
  } else if constexpr (std::is_same_v<T, QCheckBox>) {
    return &QCheckBox::toggled;
  } else if constexpr (std::is_same_v<T, QDoubleSpinBox>) {
    return QOverload<double>::of(&QDoubleSpinBox::valueChanged);
  } else if constexpr (std::is_same_v<T, QSpinBox>) {
    return QOverload<int>::of(&QSpinBox::valueChanged);
  } else if constexpr (std::is_same_v<T, QLineEdit>) {
    return &QLineEdit::textChanged;
  }
}

// --- Widget value extraction ---

template <typename T>
auto widgetValue(const T* w)
{
  if constexpr (std::is_same_v<T, QComboBox>) {
    return w->currentData().toString();
  } else if constexpr (std::is_same_v<T, QCheckBox>) {
    return w->isChecked();
  } else if constexpr (std::is_same_v<T, QDoubleSpinBox>) {
    return w->value();
  } else if constexpr (std::is_same_v<T, QSpinBox>) {
    return w->value();
  } else if constexpr (std::is_same_v<T, QLineEdit>) {
    return w->text();
  }
}

// --- Widget builders ---

void addBoolWidget(QGridLayout* layout, int row, QJsonObject& parameterNode)
{
  QJsonValue nameValue = parameterNode.value("name");
  QJsonValue labelValue = parameterNode.value("label");

  if (nameValue.isUndefined()) {
    return;
  }

  auto* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  bool defaultValue = false;
  if (parameterNode.contains("default")) {
    QJsonValue defaultNode = parameterNode.value("default");
    if (defaultNode.isBool()) {
      defaultValue = defaultNode.toBool();
    }
  }
  auto* checkBox = new QCheckBox();
  checkBox->setObjectName(nameValue.toString());
  checkBox->setCheckState(defaultValue ? Qt::Checked : Qt::Unchecked);
  label->setBuddy(checkBox);
  layout->addWidget(checkBox, row, 1, 1, 1);
}

template <typename T>
void addNumericWidget(QGridLayout* layout, int row,
                      QJsonObject& parameterNode)
{
  QJsonValue nameValue = parameterNode.value("name");
  QJsonValue labelValue = parameterNode.value("label");

  if (nameValue.isUndefined()) {
    return;
  }

  auto* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  std::vector<T> defaultValues;
  if (parameterNode.contains("default")) {
    QJsonValue defaultNode = parameterNode.value("default");
    if (isType<T>(defaultNode)) {
      defaultValues.push_back(getAs<T>(defaultNode));
    } else if (defaultNode.isArray()) {
      QJsonArray defaultArray = defaultNode.toArray();
      for (QJsonObject::size_type i = 0; i < defaultArray.size(); ++i) {
        defaultValues.push_back(getAs<T>(defaultArray[i]));
      }
    }
  }

  if (defaultValues.empty()) {
    // No default specified, use a single zero
    defaultValues.push_back(T(0));
  }

  std::vector<T> minValues(defaultValues.size(),
                           std::numeric_limits<T>::lowest());
  if (parameterNode.contains("minimum")) {
    QJsonValue minNode = parameterNode.value("minimum");
    if (isType<T>(minNode)) {
      minValues[0] = getAs<T>(minNode);
    } else if (minNode.isArray()) {
      QJsonArray minArray = minNode.toArray();
      for (QJsonObject::size_type i = 0;
           i < minArray.size() && i < (int)minValues.size(); ++i) {
        minValues[i] = getAs<T>(minArray[i]);
      }
    }
  }

  std::vector<T> maxValues(defaultValues.size(),
                           std::numeric_limits<T>::max());
  if (parameterNode.contains("maximum")) {
    QJsonValue maxNode = parameterNode.value("maximum");
    if (isType<T>(maxNode)) {
      maxValues[0] = getAs<T>(maxNode);
    } else if (maxNode.isArray()) {
      QJsonArray maxArray = maxNode.toArray();
      for (QJsonObject::size_type i = 0;
           i < maxArray.size() && i < (int)maxValues.size(); ++i) {
        maxValues[i] = getAs<T>(maxArray[i]);
      }
    }
  }

  int precision = -1;
  if (parameterNode.contains("precision")) {
    QJsonValue precNode = parameterNode.value("precision");
    if (isType<int>(precNode)) {
      precision = getAs<int>(precNode);
    }
  }
  T step = -1;
  if (parameterNode.contains("step")) {
    QJsonValue stepNode = parameterNode.value("step");
    if (isType<T>(stepNode)) {
      step = getAs<T>(stepNode);
    }
  }

  auto* horizontalLayout = new QHBoxLayout();
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  auto* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  label->setBuddy(horizontalWidget);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  for (size_t i = 0; i < defaultValues.size(); ++i) {
    QString name = nameValue.toString();
    if (defaultValues.size() > 1) {
      name.append("#%1");
      name = name.arg(i, 3, 10, QLatin1Char('0'));
    }

    QWidget* spinBox = getNumericWidget(defaultValues[i], minValues[i],
                                        maxValues[i], precision, step);
    spinBox->setObjectName(name);
    horizontalLayout->addWidget(spinBox);
  }
}

void addEnumerationWidget(QGridLayout* layout, int row,
                          QJsonObject& parameterNode)
{
  // Copies, not QJsonValueRefs: operator[] on a missing key ("label" is
  // optional) inserts it, which invalidates every ref taken earlier and
  // left the widget's objectName empty.
  QJsonValue nameValue = parameterNode.value("name");
  QJsonValue labelValue = parameterNode.value("label");

  if (nameValue.isUndefined()) {
    return;
  }

  auto* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  auto* comboBox = new QComboBox();
  comboBox->setObjectName(nameValue.toString());
  label->setBuddy(comboBox);
  QJsonValue optionsNode = parameterNode.value("options");
  if (!optionsNode.isUndefined()) {
    QJsonArray optionsArray = optionsNode.toArray();
    for (QJsonObject::size_type i = 0; i < optionsArray.size(); ++i) {
      QJsonObject optionNode = optionsArray[i].toObject();
      // An option is a single {"Label": value} pair. Anything else — a
      // bare value, or the empty object you get from writing an
      // undefined value into a QJsonObject — has no keys, and keys()[0]
      // would read off the end of the list. Descriptions are hand-
      // editable, so this has to be tolerated rather than trusted.
      if (optionNode.isEmpty()) {
        continue;
      }
      QString optionName = optionNode.keys().constFirst();
      QJsonValueRef optionValueNode = optionNode[optionName];
      QVariant optionValue;
      if (isType<int>(optionValueNode)) {
        optionValue = optionValueNode.toInt();
      } else {
        optionValue = optionValueNode.toVariant();
      }
      comboBox->addItem(optionName, optionValue);
    }
  }

  QJsonValue defaultNode = parameterNode.value("default");
  if (!defaultNode.isUndefined()) {
    if (isType<int>(defaultNode)) {
      comboBox->setCurrentIndex(getAs<int>(defaultNode));
    } else if (defaultNode.isString()) {
      int defaultIndex = comboBox->findData(getAs<QString>(defaultNode));
      if (defaultIndex >= 0) {
        comboBox->setCurrentIndex(defaultIndex);
      }
    }
  }

  layout->addWidget(comboBox, row, 1, 1, 1);
}

void addXYZHeaderWidget(QGridLayout* layout, int row, const QJsonValue&)
{
  auto* horizontalLayout = new QHBoxLayout;
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  auto* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  auto* xLabel = new QLabel("X");
  xLabel->setAlignment(Qt::AlignCenter);
  horizontalLayout->addWidget(xLabel);
  auto* yLabel = new QLabel("Y");
  yLabel->setAlignment(Qt::AlignCenter);
  horizontalLayout->addWidget(yLabel);
  auto* zLabel = new QLabel("Z");
  zLabel->setAlignment(Qt::AlignCenter);
  horizontalLayout->addWidget(zLabel);
}

void addPathWidget(QGridLayout* layout, int row, QJsonObject& pathNode)
{
  auto* horizontalLayout = new QHBoxLayout;
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  auto* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  QJsonValueRef typeValue = pathNode["type"];
  if (typeValue.isUndefined()) {
    return;
  }
  QString type = typeValue.toString();

  QJsonValueRef nameValue = pathNode["name"];
  if (nameValue.isUndefined()) {
    return;
  }

  QJsonValueRef labelValue = pathNode["label"];
  auto* label = new QLabel(nameValue.toString());
  label->setBuddy(horizontalWidget);
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  auto* pathField = new QLineEdit();
  pathField->setProperty("type", type);
  pathField->setObjectName(nameValue.toString());
  pathField->setMinimumWidth(500);

  QJsonValueRef defaultNode = pathNode["default"];
  if (!defaultNode.isUndefined() && defaultNode.isString()) {
    pathField->setText(getAs<QString>(defaultNode));
  }

  horizontalLayout->addWidget(pathField);
  auto filter = pathNode["filter"].toString();

  auto* browseButton = new QPushButton("Browse");
  horizontalLayout->addWidget(browseButton);
  QObject::connect(
    browseButton, &QPushButton::clicked,
    [type, pathField, filter]() {
      QString browseDir;
      if (!pathField->text().isEmpty()) {
        QFileInfo currentValue(pathField->text());
        auto dir = currentValue.dir();
        if (dir.exists()) {
          browseDir = dir.absolutePath();
        }
      }

      QString path;
      if (type == "file") {
        path = QFileDialog::getOpenFileName(
          pathField->window(), "Select File", browseDir, filter);
      } else if (type == "save_file") {
        path = QFileDialog::getSaveFileName(
          pathField->window(), "Save File Path", browseDir, filter);
      } else {
        path = QFileDialog::getExistingDirectory(
          pathField->window(), "Select Directory", browseDir);
      }

      if (!path.isNull()) {
        pathField->setText(path);
      }
    });
}

void addStringWidget(QGridLayout* layout, int row, QJsonObject& pathNode)
{
  auto* horizontalLayout = new QHBoxLayout;
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  auto* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  QJsonValueRef typeValue = pathNode["type"];
  if (typeValue.isUndefined()) {
    return;
  }
  QString type = typeValue.toString();

  QJsonValueRef nameValue = pathNode["name"];
  if (nameValue.isUndefined()) {
    return;
  }

  QJsonValueRef labelValue = pathNode["label"];
  auto* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  auto* stringField = new QLineEdit();
  stringField->setProperty("type", type);
  stringField->setObjectName(nameValue.toString());
  stringField->setMinimumWidth(500);
  label->setBuddy(stringField);
  horizontalLayout->addWidget(stringField);

  QJsonValueRef defaultNode = pathNode["default"];
  if (!defaultNode.isUndefined() && defaultNode.isString()) {
    stringField->setText(getAs<QString>(defaultNode));
  }
}

void addSelectScalarsWidget(QGridLayout* layout, int row,
                            QJsonObject& parameterNode,
                            const QList<PortScalars>& portScalars)
{
  QJsonValue nameValue = parameterNode.value("name");
  QJsonValue labelValue = parameterNode.value("label");

  if (nameValue.isUndefined()) {
    QJsonDocument document(parameterNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QString name = nameValue.toString();

  // Resolve which input port this parameter draws from. JSON's "input"
  // field selects by port name; missing/unmatched falls back to the
  // first entry (typically the primary volume input).
  QString inputName = parameterNode.value("input").toString();
  const PortScalars* resolved = nullptr;
  if (!inputName.isEmpty()) {
    for (const auto& ps : portScalars) {
      if (ps.portName == inputName) {
        resolved = &ps;
        break;
      }
    }
  }
  if (!resolved && !portScalars.isEmpty()) {
    resolved = &portScalars.first();
  }
  const QStringList availableScalars =
    resolved ? resolved->scalarNames : QStringList{};
  const QString activeScalar =
    resolved ? resolved->activeScalar : QString{};

  auto* label = new QLabel(name);
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  auto* container = new QWidget();
  container->setObjectName(name);
  container->setProperty("type", "select_scalars");
  // Stash the active scalar name so parameterValues() can return it as
  // the parameter value when "Select active" is checked, without needing
  // VolumeData access at extraction time.
  container->setProperty("activeScalar", activeScalar);
  // Fixed vertical policy so the parent grid doesn't squish the combo box
  // when total dialog height is tight — keep the row at its sizeHint.
  container->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  label->setBuddy(container);

  auto* vLayout = new QVBoxLayout();
  vLayout->setContentsMargins(0, 0, 0, 0);
  container->setLayout(vLayout);

  bool showShortcuts = parameterNode.value("show_apply_all").toBool(true);
  // Wrap the two shortcut checkboxes in a fixed-width widget so the row
  // doesn't propagate an "expanding" horizontal policy up and force the
  // enclosing column to claim all available dialog width.
  auto* shortcutsWidget = new QWidget();
  shortcutsWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  auto* shortcutsRow = new QHBoxLayout(shortcutsWidget);
  shortcutsRow->setContentsMargins(0, 0, 0, 0);

  auto* selectAllCheckBox = new QCheckBox("Select all");
  selectAllCheckBox->setObjectName(name + "_select_all");
  selectAllCheckBox->setChecked(showShortcuts);
  selectAllCheckBox->setVisible(showShortcuts);
  shortcutsRow->addWidget(selectAllCheckBox);

  auto* selectActiveCheckBox = new QCheckBox("Select active");
  selectActiveCheckBox->setObjectName(name + "_select_active");
  selectActiveCheckBox->setChecked(false);
  selectActiveCheckBox->setVisible(showShortcuts && !activeScalar.isEmpty());
  shortcutsRow->addWidget(selectActiveCheckBox);

  vLayout->addWidget(shortcutsWidget, 0, Qt::AlignLeft);

  auto* comboBox = new QComboBox();
  comboBox->setObjectName(name + "_combo");
  auto* model = new QStandardItemModel(comboBox);
  comboBox->setModel(model);
  comboBox->setEnabled(!selectAllCheckBox->isChecked() &&
                      !selectActiveCheckBox->isChecked());
  // Editable + read-only QLineEdit so we can drive the collapsed display
  // text ourselves (the joined list of checked items). QComboBox's default
  // display just shows currentIndex's label, which isn't useful here.
  comboBox->setEditable(true);
  comboBox->lineEdit()->setReadOnly(true);
  comboBox->lineEdit()->setFocusPolicy(Qt::NoFocus);

  for (const QString& scalar : availableScalars) {
    auto* item = new QStandardItem(scalar);
    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    item->setData(Qt::Checked, Qt::CheckStateRole);
    model->appendRow(item);
  }

  // Restore previous selection from "default" if present
  QJsonValue defaultNode = parameterNode.value("default");
  if (!defaultNode.isUndefined() && defaultNode.isArray()) {
    QJsonArray defaultArray = defaultNode.toArray();
    QSet<QString> selected;
    for (const auto& v : defaultArray) {
      selected.insert(v.toString());
    }

    bool allSelected = true;
    for (int i = 0; i < model->rowCount(); ++i) {
      bool isSelected = selected.contains(model->item(i)->text());
      model->item(i)->setData(isSelected ? Qt::Checked : Qt::Unchecked,
                              Qt::CheckStateRole);
      if (!isSelected) {
        allSelected = false;
      }
    }
    selectAllCheckBox->setChecked(showShortcuts && allSelected);
    comboBox->setEnabled(!selectAllCheckBox->isChecked());
  }

  if (availableScalars.size() <= 1) {
    label->setVisible(false);
    container->setVisible(false);
  }

  vLayout->addWidget(comboBox);

  auto updateDisplayText =
    [comboBox, model, selectAllCheckBox, selectActiveCheckBox,
     activeScalar]() {
    QStringList selected;
    if (selectActiveCheckBox->isChecked()) {
      if (!activeScalar.isEmpty()) {
        selected << activeScalar;
      }
    } else if (selectAllCheckBox->isChecked()) {
      for (int i = 0; i < model->rowCount(); ++i) {
        selected << model->item(i)->text();
      }
    } else {
      for (int i = 0; i < model->rowCount(); ++i) {
        auto* item = model->item(i);
        if (item->checkState() == Qt::Checked) {
          selected << item->text();
        }
      }
    }
    auto* le = comboBox->lineEdit();
    le->setText(selected.join(", "));
    // Park the cursor at the start so a too-long string truncates on the
    // right (visible head) instead of the left (visible tail).
    le->setCursorPosition(0);
  };

  auto updateComboEnabled =
    [comboBox, selectAllCheckBox, selectActiveCheckBox]() {
    comboBox->setEnabled(!selectAllCheckBox->isChecked() &&
                         !selectActiveCheckBox->isChecked());
  };

  // Deferred via a 0-ms timer because QComboBox internally reacts to the
  // model's dataChanged signal (after a check-state toggle) and rewrites
  // the line edit text to the clicked item's label. Running after the
  // event loop spins ensures our joined text wins.
  QObject::connect(model, &QStandardItemModel::itemChanged, comboBox,
                   [updateDisplayText](QStandardItem*) {
    QTimer::singleShot(0, updateDisplayText);
  });

  // Mutual exclusivity: when either shortcut is checked, the other one
  // turns off. QSignalBlocker prevents the other slot from firing back.
  QObject::connect(selectAllCheckBox, &QCheckBox::toggled, container,
                   [selectActiveCheckBox, updateComboEnabled,
                    updateDisplayText](bool checked) {
    if (checked) {
      QSignalBlocker blocker(selectActiveCheckBox);
      selectActiveCheckBox->setChecked(false);
    }
    updateComboEnabled();
    updateDisplayText();
  });
  QObject::connect(selectActiveCheckBox, &QCheckBox::toggled, container,
                   [selectAllCheckBox, updateComboEnabled,
                    updateDisplayText](bool checked) {
    if (checked) {
      QSignalBlocker blocker(selectAllCheckBox);
      selectAllCheckBox->setChecked(false);
    }
    updateComboEnabled();
    updateDisplayText();
  });

  // Clicking the (read-only) line edit opens the popup, so the field
  // behaves like the rest of the combo box rather than a dead text area.
  // We open on release, not press: opening on press lets the in-flight
  // release land on the already-open popup, where it's interpreted as
  // "released outside any item, close" — same effect even if showPopup()
  // is deferred, since the user may still be holding the button when the
  // timer fires.
  class LineEditOpensPopupFilter : public QObject
  {
  public:
    LineEditOpensPopupFilter(QComboBox* combo, QObject* parent)
      : QObject(parent), m_combo(combo) {}
    bool eventFilter(QObject* /*obj*/, QEvent* event) override
    {
      if (!m_combo->isEnabled()) {
        return false;
      }
      if (event->type() == QEvent::MouseButtonPress) {
        return true;
      }
      if (event->type() == QEvent::MouseButtonRelease) {
        m_combo->showPopup();
        return true;
      }
      return false;
    }
  private:
    QComboBox* m_combo;
  };
  comboBox->lineEdit()->installEventFilter(
    new LineEditOpensPopupFilter(comboBox, comboBox));

  updateDisplayText();

  // Keep the combo box popup open while the user ticks/un-ticks items.
  // Consume the press; on release, toggle the item under the cursor only
  // if a matching press was seen on the viewport (ignores the orphaned
  // release from the click that originally opened the popup).
  class ComboEventFilter : public QObject
  {
  public:
    ComboEventFilter(QComboBox* combo, QObject* parent)
      : QObject(parent), m_combo(combo) {}
    bool eventFilter(QObject* /*obj*/, QEvent* event) override
    {
      if (event->type() == QEvent::MouseButtonPress) {
        m_pressedOnViewport = true;
        return true;
      }
      if (event->type() == QEvent::MouseButtonRelease) {
        if (!m_pressedOnViewport) {
          return true;
        }
        m_pressedOnViewport = false;
        auto* view = m_combo->view();
        auto index = view->indexAt(
          static_cast<QMouseEvent*>(event)->pos());
        if (index.isValid()) {
          auto* model = qobject_cast<QStandardItemModel*>(m_combo->model());
          if (model) {
            auto* item = model->itemFromIndex(index);
            if (item && (item->flags() & Qt::ItemIsUserCheckable)) {
              auto state = item->checkState() == Qt::Checked
                             ? Qt::Unchecked : Qt::Checked;
              item->setCheckState(state);
            }
          }
        }
        return true;
      }
      return false;
    }
  private:
    QComboBox* m_combo;
    bool m_pressedOnViewport = false;
  };

  auto* filter = new ComboEventFilter(comboBox, comboBox);
  comboBox->view()->viewport()->installEventFilter(filter);

  layout->addWidget(container, row, 1, 1, 1);
}

static const QStringList PATH_TYPES = { "file", "save_file", "directory" };

// --- enable_if / visible_if support ---

template <typename T>
bool compareGeneric(T value, T ref, const QString& comparator)
{
  if (comparator == "==") {
    return value == ref;
  } else if (comparator == "!=") {
    return value != ref;
  }
  return false;
}

template <typename T>
bool compareNumbers(T value, T ref, const QString& comparator)
{
  if (comparator == "==") {
    return value == ref;
  } else if (comparator == "!=") {
    return value != ref;
  } else if (comparator == ">") {
    return value > ref;
  } else if (comparator == "<") {
    return value < ref;
  } else if (comparator == ">=") {
    return value >= ref;
  } else if (comparator == "<=") {
    return value <= ref;
  }
  return false;
}

template <typename T>
bool compare(const T* widget, const QVariant& compareValue,
             const QString& comparator)
{
  auto value = widgetValue(widget);
  if constexpr (std::is_same_v<T, QComboBox> ||
                std::is_same_v<T, QLineEdit>) {
    QString ref = compareValue.toString();
    if (ref.startsWith("'") || ref.startsWith("\"")) {
      ref = ref.mid(1, ref.length() - 2);
    }
    return compareGeneric(value, ref, comparator);
  } else if constexpr (std::is_same_v<T, QCheckBox>) {
    return compareGeneric(value, compareValue.toBool(), comparator);
  } else if constexpr (std::is_same_v<T, QSpinBox>) {
    return compareNumbers(value, compareValue.toInt(), comparator);
  } else if constexpr (std::is_same_v<T, QDoubleSpinBox>) {
    return compareNumbers(value, compareValue.toDouble(), comparator);
  }
  return false;
}

struct EnableCondition
{
  QWidget* refWidget = nullptr;
  QString comparator;
  QVariant compareValue;
};

static bool evaluateCondition(const EnableCondition& cond)
{
  auto* w = cond.refWidget;
  if (isWidgetType<QSpinBox>(w)) {
    return compare(qobject_cast<QSpinBox*>(w), cond.compareValue,
                   cond.comparator);
  } else if (isWidgetType<QDoubleSpinBox>(w)) {
    return compare(qobject_cast<QDoubleSpinBox*>(w), cond.compareValue,
                   cond.comparator);
  } else if (isWidgetType<QCheckBox>(w)) {
    return compare(qobject_cast<QCheckBox*>(w), cond.compareValue,
                   cond.comparator);
  } else if (isWidgetType<QComboBox>(w)) {
    return compare(qobject_cast<QComboBox*>(w), cond.compareValue,
                   cond.comparator);
  } else if (isWidgetType<QLineEdit>(w)) {
    return compare(qobject_cast<QLineEdit*>(w), cond.compareValue,
                   cond.comparator);
  }
  return false;
}

static bool evaluateCompound(
  const QList<QList<EnableCondition>>& orGroups)
{
  for (auto& andGroup : orGroups) {
    bool groupResult = true;
    for (auto& cond : andGroup) {
      if (!evaluateCondition(cond)) {
        groupResult = false;
        break;
      }
    }
    if (groupResult) {
      return true;
    }
  }
  return false;
}

QWidget* findRootInterfaceWidget(const QWidget* widget)
{
  auto* parent = widget->parent();
  while (parent &&
         !parent->property("isRootInterfaceWidget").toBool()) {
    parent = parent->parent();
  }

  if (parent && parent->property("isRootInterfaceWidget").toBool()) {
    return qobject_cast<QWidget*>(parent);
  }
  return nullptr;
}

QLabel* findLabelForWidget(const QWidget* widget)
{
  auto* parent = findRootInterfaceWidget(widget);
  if (!parent) {
    return nullptr;
  }

  for (auto* child : parent->findChildren<QLabel*>()) {
    if (child->buddy() == widget) {
      return child;
    }
  }
  return nullptr;
}

void setWidgetProperty(QWidget* widget, const char* property, QVariant value)
{
  QWidget* field = widget;
  if (isWidgetNumeric(widget) || isWidgetType<QLineEdit>(widget)) {
    widget = widget->parentWidget();
    if (!widget) {
      return;
    }
  }

  widget->setProperty(property, value);

  // The label is the buddy of the row's container for some parameter
  // types and of the field itself for others (strings)
  auto* label = findLabelForWidget(widget);
  if (!label && field != widget) {
    label = findLabelForWidget(field);
  }
  if (label) {
    label->setProperty(property, value);
  }
}

static void connectWidgetChanged(QWidget* refWidget, QWidget* target,
                                 std::function<void()> func)
{
  if (isWidgetType<QSpinBox>(refWidget)) {
    target->connect(qobject_cast<QSpinBox*>(refWidget),
                    changedSignal<QSpinBox>(), target, func);
  } else if (isWidgetType<QDoubleSpinBox>(refWidget)) {
    target->connect(qobject_cast<QDoubleSpinBox*>(refWidget),
                    changedSignal<QDoubleSpinBox>(), target, func);
  } else if (isWidgetType<QCheckBox>(refWidget)) {
    target->connect(qobject_cast<QCheckBox*>(refWidget),
                    changedSignal<QCheckBox>(), target, func);
  } else if (isWidgetType<QComboBox>(refWidget)) {
    target->connect(qobject_cast<QComboBox*>(refWidget),
                    changedSignal<QComboBox>(), target, func);
  } else if (isWidgetType<QLineEdit>(refWidget)) {
    target->connect(qobject_cast<QLineEdit*>(refWidget),
                    changedSignal<QLineEdit>(), target, func);
  }
}

} // end anonymous namespace

namespace tomviz {
namespace pipeline {

ParameterInterfaceBuilder::ParameterInterfaceBuilder(QObject* parentObject)
  : QObject(parentObject)
{}

void ParameterInterfaceBuilder::setJSONDescription(const QString& json)
{
  setJSONDescription(QJsonDocument::fromJson(json.toLatin1()));
}

void ParameterInterfaceBuilder::setJSONDescription(const QJsonDocument& doc)
{
  if (!doc.isObject()) {
    qCritical() << "Failed to parse operator JSON";
    m_json = QJsonDocument();
  } else {
    m_json = doc;
  }
}

void ParameterInterfaceBuilder::setParameterValues(
  const QMap<QString, QVariant>& values)
{
  m_parameterValues = values;
}

void ParameterInterfaceBuilder::setPortScalars(
  const QList<PortScalars>& ports)
{
  m_portScalars = ports;
}

QWidget* ParameterInterfaceBuilder::buildWidget(QWidget* parent) const
{
  auto* widget = new QWidget(parent);
  widget->setProperty("isRootInterfaceWidget", true);

  auto* verticalLayout = new QVBoxLayout;
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  widget->setLayout(verticalLayout);

  auto* gridContainer = new QWidget;
  verticalLayout->addWidget(gridContainer);
  verticalLayout->addStretch();

  auto* layout = new QGridLayout;
  gridContainer->setLayout(layout);

  if (!m_json.isObject()) {
    return widget;
  }
  QJsonObject root = m_json.object();

  // Description label. Nothing else renders the node's "description",
  // and SAM2SeedWidget::wireForm() reaches in here to repair this
  // label's size policy, so it has to stay.
  QJsonValueRef descriptionValue = root["description"];
  if (!descriptionValue.isUndefined()) {
    auto* descLabel = new QLabel(descriptionValue.toString());
    descLabel->setWordWrap(true);
    descLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    verticalLayout->insertWidget(0, descLabel);
  }

  // Parameters
  QJsonValueRef parametersNode = root["parameters"];
  if (parametersNode.isUndefined()) {
    return widget;
  }
  auto parameters = parametersNode.toArray();

  // Use a non-const copy of this for the mutable methods
  auto* self =
    const_cast<ParameterInterfaceBuilder*>(this);
  self->buildParameterInterface(layout, parameters);

  return widget;
}

void ParameterInterfaceBuilder::buildParameterInterface(
  QGridLayout* layout, QJsonArray& parameters) const
{
  QJsonObject::size_type numParameters = parameters.size();
  for (QJsonObject::size_type i = 0; i < numParameters; ++i) {
    QJsonValueRef parameterNode = parameters[i];
    QJsonObject parameterObject = parameterNode.toObject();

    QJsonValueRef typeValue = parameterObject["type"];
    if (typeValue.isUndefined()) {
      qWarning() << "Parameter has no type entry";
      continue;
    }

    QString typeString = typeValue.toString();

    // Override default with stored parameter value
    QJsonValueRef nameValue = parameterObject["name"];
    if (!nameValue.isUndefined()) {
      QString parameterName = nameValue.toString();
      if (m_parameterValues.contains(parameterName)) {
        QVariant parameterValue = m_parameterValues[parameterName];
        if (typeString == "enumeration") {
          // Stored values are option values; the widget reads a numeric
          // default as an option index, so translate
          int index = PythonNodeUtils::enumOptionIndex(
            parameterValue, parameterObject["options"].toArray());
          if (index >= 0) {
            parameterValue = index;
          }
        }
        parameterObject["default"] =
          QJsonValue::fromVariant(parameterValue);
      }
    }

    if (typeString == "bool") {
      addBoolWidget(layout, i + 1, parameterObject);
    } else if (typeString == "int") {
      addNumericWidget<int>(layout, i + 1, parameterObject);
    } else if (typeString == "double") {
      addNumericWidget<double>(layout, i + 1, parameterObject);
    } else if (typeString == "enumeration") {
      addEnumerationWidget(layout, i + 1, parameterObject);
    } else if (typeString == "xyz_header") {
      addXYZHeaderWidget(layout, i + 1, parameterObject);
    } else if (PATH_TYPES.contains(typeString)) {
      addPathWidget(layout, i + 1, parameterObject);
    } else if (typeString == "string") {
      addStringWidget(layout, i + 1, parameterObject);
    } else if (typeString == "select_scalars") {
      addSelectScalarsWidget(layout, i + 1, parameterObject, m_portScalars);
    }
    // Note: "dataset" type is not supported in the pipeline lib
    // (it depends on DataSource/ModuleManager).
  }

  setupEnableAndVisibleStates(layout->parentWidget(), parameters);
}

void ParameterInterfaceBuilder::setupEnableAndVisibleStates(
  const QObject* parent, QJsonArray& parameters) const
{
  setupEnableStates(parent, parameters, true);
  setupEnableStates(parent, parameters, false);
}

void ParameterInterfaceBuilder::setupEnableStates(
  const QObject* parent, QJsonArray& parameters, bool visible) const
{
  static const QStringList validComparators = { "==", "!=", ">",
                                                ">=", "<", "<=" };

  QJsonObject::size_type numParameters = parameters.size();
  for (QJsonObject::size_type i = 0; i < numParameters; ++i) {
    QJsonValueRef parameterNode = parameters[i];
    QJsonObject parameterObject = parameterNode.toObject();

    QString text = visible ? "visible_if" : "enable_if";
    QString enableIfValue = parameterObject[text].toString("");
    if (enableIfValue.isEmpty()) {
      continue;
    }

    QString widgetName = parameterObject["name"].toString("");
    if (widgetName.isEmpty()) {
      continue;
    }
    auto* widget = parent->findChild<QWidget*>(widgetName);
    if (!widget) {
      continue;
    }

    auto orParts = enableIfValue.simplified().split(
      " or ", Qt::KeepEmptyParts, Qt::CaseInsensitive);

    QList<QList<EnableCondition>> orGroups;
    bool parseError = false;

    for (auto& orPart : orParts) {
      auto andParts = orPart.simplified().split(
        " and ", Qt::KeepEmptyParts, Qt::CaseInsensitive);
      QList<EnableCondition> andGroup;
      for (auto& clause : andParts) {
        auto tokens = clause.simplified().split(" ");
        if (tokens.size() != 3) {
          parseError = true;
          break;
        }

        auto refWidgetName = tokens[0];
        auto comparator = tokens[1];
        auto compareValue = tokens[2];

        auto* refWidget = parent->findChild<QWidget*>(refWidgetName);
        if (!refWidget) {
          parseError = true;
          break;
        }

        if (!validComparators.contains(comparator)) {
          parseError = true;
          break;
        }

        EnableCondition cond;
        cond.refWidget = refWidget;
        cond.comparator = comparator;
        cond.compareValue = compareValue;
        andGroup.append(cond);
      }

      if (parseError) {
        break;
      }
      orGroups.append(andGroup);
    }

    if (parseError) {
      continue;
    }

    const char* property = visible ? "visible" : "enabled";

    auto evalFunc = [orGroups, widget, property]() {
      bool result = evaluateCompound(orGroups);
      setWidgetProperty(widget, property, result);
    };

    QSet<QWidget*> connectedWidgets;
    for (auto& andGroup : orGroups) {
      for (auto& cond : andGroup) {
        if (connectedWidgets.contains(cond.refWidget)) {
          continue;
        }
        connectedWidgets.insert(cond.refWidget);
        connectWidgetChanged(cond.refWidget, widget, evalFunc);
      }
    }

    // Evaluate once for the initial state
    evalFunc();
  }
}

QMap<QString, QVariant> ParameterInterfaceBuilder::parameterValues(
  const QWidget* parent)
{
  QMap<QString, QVariant> map;

  // Handle select_scalars containers first, and record the names of
  // their internal widgets so the generic loops below skip them.
  QSet<QString> selectScalarsInternalNames;
  for (auto* w : parent->findChildren<QWidget*>()) {
    if (w->property("type").toString() != "select_scalars") {
      continue;
    }
    QString name = w->objectName();
    auto* selectAllCB = w->findChild<QCheckBox*>(name + "_select_all");
    auto* selectActiveCB = w->findChild<QCheckBox*>(name + "_select_active");
    auto* combo = w->findChild<QComboBox*>(name + "_combo");
    if (!selectAllCB || !selectActiveCB || !combo) {
      continue;
    }
    selectScalarsInternalNames.insert(selectAllCB->objectName());
    selectScalarsInternalNames.insert(selectActiveCB->objectName());
    selectScalarsInternalNames.insert(combo->objectName());

    auto* model = qobject_cast<QStandardItemModel*>(combo->model());
    if (!model) {
      continue;
    }

    QVariantList selectedScalars;
    if (selectActiveCB->isChecked()) {
      QString activeScalar = w->property("activeScalar").toString();
      if (!activeScalar.isEmpty()) {
        selectedScalars << activeScalar;
      }
    } else if (selectAllCB->isChecked()) {
      for (int i = 0; i < model->rowCount(); ++i) {
        selectedScalars << model->item(i)->text();
      }
    } else {
      for (int i = 0; i < model->rowCount(); ++i) {
        if (model->item(i)->checkState() == Qt::Checked) {
          selectedScalars << model->item(i)->text();
        }
      }
    }
    map[name] = selectedScalars;
  }

  // Checkboxes
  QList<QCheckBox*> checkBoxes = parent->findChildren<QCheckBox*>();
  for (auto* cb : checkBoxes) {
    if (selectScalarsInternalNames.contains(cb->objectName())) {
      continue;
    }
    map[cb->objectName()] = (cb->checkState() == Qt::Checked);
  }

  // QSpinBox
  QList<QSpinBox*> spinBoxes = parent->findChildren<QSpinBox*>();
  for (auto* sb : spinBoxes) {
    map[sb->objectName()] = sb->value();
  }

  // QDoubleSpinBox
  QList<QDoubleSpinBox*> doubleSpinBoxes =
    parent->findChildren<QDoubleSpinBox*>();
  for (auto* dsb : doubleSpinBoxes) {
    map[dsb->objectName()] = dsb->value();
  }

  // QComboBox
  QList<QComboBox*> comboBoxes = parent->findChildren<QComboBox*>();
  for (auto* combo : comboBoxes) {
    if (selectScalarsInternalNames.contains(combo->objectName())) {
      continue;
    }
    int currentIndex = combo->currentIndex();
    map[combo->objectName()] = combo->itemData(currentIndex);
  }

  // Assemble multi-component properties (name#000, name#001, ...)
  QMap<QString, QVariant>::iterator iter = map.begin();
  while (iter != map.end()) {
    QString name = iter.key();
    QVariant value = iter.value();
    int poundIndex = name.indexOf("#");
    if (poundIndex >= 0) {
      name = name.left(poundIndex);

      QList<QVariant> valueList;
      auto findIter = map.find(name);
      if (findIter != map.end()) {
        valueList = map[name].toList();
      }

      valueList.append(value);
      map[name] = valueList;

      iter = map.erase(iter);
    } else {
      ++iter;
    }
  }

  // QLineEdits (file, save_file, directory, string types)
  QList<QLineEdit*> lineEdits = parent->findChildren<QLineEdit*>();
  for (auto* lineEdit : lineEdits) {
    QVariant type = lineEdit->property("type");
    bool canConvert = QMetaType::canConvert(
      type.metaType(), QMetaType(QMetaType::QString));
    if (canConvert &&
        (PATH_TYPES.contains(type.toString()) ||
         type.toString() == "string")) {
      map[lineEdit->objectName()] = lineEdit->text();
    }
  }

  return map;
}

} // namespace pipeline
} // namespace tomviz

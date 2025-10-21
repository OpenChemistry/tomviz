/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "InterfaceBuilder.h"

#include "ActiveObjects.h"
#include "DataSource.h"
#include "DoubleSpinBox.h"
#include "ModuleManager.h"
#include "SpinBox.h"
#include "Utilities.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QWidget>

using tomviz::DataSource;

Q_DECLARE_METATYPE(DataSource*)

namespace {

// Templated query of QJsonValue type
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

// Templated accessor of QJsonValue value
template <typename T>
T getAs(const QJsonValue&)
{
  return 0;
}

template <>
int getAs(const QJsonValue& value)
{
  int iValue = 0;
  try {
    iValue = value.toInt();
  } catch (...) {
    qCritical() << "Could not get int from QJsonValue";
  }
  return iValue;
}

template <>
double getAs(const QJsonValue& value)
{
  double dValue = 0.0;
  try {
    dValue = value.toDouble();
  } catch (...) {
    qCritical() << "Could not get double from QJsonValue";
  }
  return dValue;
}

template <>
QString getAs(const QJsonValue& value)
{
  QString strValue;
  try {
    strValue = value.toString();
  } catch (...) {
    qCritical() << "Could not get QString from QJsonValue";
  }
  return strValue;
}

// Templated generation of numeric editing widgets.
template <typename T>
QWidget* getNumericWidget(T, T, T, int, T)
{
  return nullptr;
}

template <>
QWidget* getNumericWidget(int defaultValue, int rangeMin, int rangeMax,
                          int /*precision*/, int step)
{
  tomviz::SpinBox* spinBox = new tomviz::SpinBox();
  if (step == -1) {
    spinBox->setSingleStep(1);
  } else {
    spinBox->setSingleStep(step);
  }
  spinBox->setMinimum(rangeMin);
  spinBox->setMaximum(rangeMax);
  spinBox->setValue(defaultValue);
  return spinBox;
}

template <>
QWidget* getNumericWidget(double defaultValue, double rangeMin, double rangeMax,
                          int precision, double step)
{
  tomviz::DoubleSpinBox* spinBox = new tomviz::DoubleSpinBox();
  if (step == -1) {
    spinBox->setSingleStep(0.5);
  } else {
    spinBox->setSingleStep(step);
  }
  if (precision != -1) {
    spinBox->setDecimals(precision);
  } else {
    spinBox->setDecimals(3);
  }
  spinBox->setMinimum(rangeMin);
  spinBox->setMaximum(rangeMax);
  spinBox->setValue(defaultValue);
  return spinBox;
}

template<typename T>
bool isWidgetType(const QWidget* widget) {
  return qobject_cast<const T*>(widget) != nullptr;
}

bool isWidgetNumeric(const QWidget* widget) {
  return isWidgetType<QDoubleSpinBox>(widget) || isWidgetType<QSpinBox>(widget);
}

template <typename T>
auto changedSignal()
{
  // Get a generic symbol a widget has changed
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

template <typename T>
auto widgetValue(const T* w)
{
  // Get the widget value.
  // This will be an int for QSpinBox, double for QDoubleSpinBox,
  // string for QComboBox, etc.
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

void addBoolWidget(QGridLayout* layout, int row, QJsonObject& parameterNode)
{
  QJsonValueRef nameValue = parameterNode["name"];
  QJsonValueRef labelValue = parameterNode["label"];

  if (nameValue.isUndefined()) {
    QJsonDocument document(parameterNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QLabel* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  bool defaultValue = false;
  if (parameterNode.contains("default")) {
    QJsonValueRef defaultNode = parameterNode["default"];
    if (defaultNode.isBool()) {
      defaultValue = defaultNode.toBool();
    }
  }
  QCheckBox* checkBox = new QCheckBox();
  checkBox->setObjectName(nameValue.toString());
  checkBox->setCheckState(defaultValue ? Qt::Checked : Qt::Unchecked);
  label->setBuddy(checkBox);
  layout->addWidget(checkBox, row, 1, 1, 1);
}

template <typename T>
void addNumericWidget(QGridLayout* layout, int row, QJsonObject& parameterNode,
                      DataSource* dataSource)
{
  QJsonValueRef nameValue = parameterNode["name"];
  QJsonValueRef labelValue = parameterNode["label"];

  if (nameValue.isUndefined()) {
    QJsonDocument document(parameterNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QLabel* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  std::vector<T> defaultValues;
  if (parameterNode.contains("default")) {
    QJsonValueRef defaultNode = parameterNode["default"];
    if (isType<T>(defaultNode)) {
      defaultValues.push_back(getAs<T>(defaultNode));
    } else if (defaultNode.isArray()) {
      QJsonArray defaultArray = defaultNode.toArray();
      for (QJsonObject::size_type i = 0; i < defaultArray.size(); ++i) {
        defaultValues.push_back(getAs<T>(defaultArray[i]));
      }
    }
  } else if (parameterNode.contains("data-default")) {
    if (!dataSource) {
      return;
    }
    QJsonValueRef defaultNode = parameterNode["data-default"];
    int extent[6];
    dataSource->getExtent(extent);
    if (defaultNode.toString() == "num-voxels-x") {
      defaultValues.push_back(extent[1] - extent[0] + 1);
    } else if (defaultNode.toString() == "num-voxels-y") {
      defaultValues.push_back(extent[3] - extent[2] + 1);
    } else if (defaultNode.toString() == "num-voxels-z") {
      defaultValues.push_back(extent[5] - extent[4] + 1);
    }
  }

  std::vector<T> minValues(defaultValues.size(), std::numeric_limits<T>::lowest());
  if (parameterNode.contains("minimum")) {
    QJsonValueRef minNode = parameterNode["minimum"];
    if (isType<T>(minNode)) {
      minValues[0] = getAs<T>(minNode);
    } else if (minNode.isArray()) {
      QJsonArray minArray = minNode.toArray();
      for (QJsonObject::size_type i = 0; i < minArray.size(); ++i) {
        minValues[i] = getAs<T>(minArray[i]);
      }
    }
  }

  std::vector<T> maxValues(defaultValues.size(), std::numeric_limits<T>::max());
  if (parameterNode.contains("maximum")) {
    QJsonValueRef maxNode = parameterNode["maximum"];
    if (isType<T>(maxNode)) {
      maxValues[0] = getAs<T>(maxNode);
    } else if (maxNode.isArray()) {
      QJsonArray maxArray = maxNode.toArray();
      for (QJsonObject::size_type i = 0; i < maxArray.size(); ++i) {
        maxValues[i] = getAs<T>(maxArray[i]);
      }
    }
  }

  int precision = -1;
  if (parameterNode.contains("precision")) {
    QJsonValueRef precNode = parameterNode["precision"];
    if (isType<int>(precNode)) {
      precision = getAs<int>(precNode);
    }
  }
  T step = -1;
  if (parameterNode.contains("step")) {
    QJsonValueRef stepNode = parameterNode["step"];
    if (isType<T>(stepNode)) {
      step = getAs<T>(stepNode);
    }
  }

  QHBoxLayout* horizontalLayout = new QHBoxLayout();
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  QWidget* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  label->setBuddy(horizontalWidget);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  for (size_t i = 0; i < defaultValues.size(); ++i) {
    QString name = nameValue.toString();
    if (defaultValues.size() > 1) {
      // Multi-element parameters are named with the pattern 'basename#XXX'
      // where 'basename' is the name of the parameter and 'XXX' is the
      // element number.
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
  QJsonValueRef nameValue = parameterNode["name"];
  QJsonValueRef labelValue = parameterNode["label"];

  if (nameValue.isUndefined()) {
    QJsonDocument document(parameterNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QLabel* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  QComboBox* comboBox = new QComboBox();
  comboBox->setObjectName(nameValue.toString());
  label->setBuddy(comboBox);
  QJsonValueRef optionsNode = parameterNode["options"];
  if (!optionsNode.isUndefined()) {
    QJsonArray optionsArray = optionsNode.toArray();
    for (QJsonObject::size_type i = 0; i < optionsArray.size(); ++i) {
      QJsonObject optionNode = optionsArray[i].toObject();
      QString optionName = optionNode.keys()[0];
      QJsonValueRef optionValueNode = optionNode[optionName];
      QVariant optionValue;
      if (isType<int>(optionValueNode)) {
        // Convert to an int if possible
        optionValue = optionValueNode.toInt();
      } else {
        // Otherwise, let it be whatever type it is...
        optionValue = optionValueNode.toVariant();
      }
      comboBox->addItem(optionName, optionValue);
    }
  }

  // Set the default if present
  QJsonValueRef defaultNode = parameterNode["default"];
  if (!defaultNode.isUndefined()) {
    if (isType<int>(defaultNode)) {
      comboBox->setCurrentIndex(getAs<int>(defaultNode));
    } else if (defaultNode.isString()) {
      // Find the data that matches, and set it
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
  QHBoxLayout* horizontalLayout = new QHBoxLayout;
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  QWidget* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  QLabel* xLabel = new QLabel("X");
  xLabel->setAlignment(Qt::AlignCenter);
  horizontalLayout->addWidget(xLabel);
  QLabel* yLabel = new QLabel("Y");
  yLabel->setAlignment(Qt::AlignCenter);
  horizontalLayout->addWidget(yLabel);
  QLabel* zLabel = new QLabel("Z");
  zLabel->setAlignment(Qt::AlignCenter);
  horizontalLayout->addWidget(zLabel);
}

void addPathWidget(QGridLayout* layout, int row, QJsonObject& pathNode)
{
  QHBoxLayout* horizontalLayout = new QHBoxLayout;
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  QWidget* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  QJsonValueRef typeValue = pathNode["type"];
  if (typeValue.isUndefined()) {
    QJsonDocument document(pathNode);
    qWarning() << QString("Parameter %1 has no type. Skipping.")
                    .arg(document.toJson().data());
    return;
  }
  QString type = typeValue.toString();

  QJsonValueRef nameValue = pathNode["name"];
  if (nameValue.isUndefined()) {
    QJsonDocument document(pathNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QJsonValueRef labelValue = pathNode["label"];
  QLabel* label = new QLabel(nameValue.toString());
  label->setBuddy(horizontalWidget);
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  QLineEdit* pathField = new QLineEdit();
  // Tag the line edit with the type, so we can distinguish it from other line
  // edit uses ( such as in a QSpinBox )
  pathField->setProperty("type", type);
  pathField->setObjectName(nameValue.toString());
  pathField->setMinimumWidth(500);

  // Set default if present
  QJsonValueRef defaultNode = pathNode["default"];
  if (!defaultNode.isUndefined() && defaultNode.isString()) {
    auto defaultValue = getAs<QString>(defaultNode);
    pathField->setText(defaultValue);
  }

  horizontalLayout->addWidget(pathField);
  auto filter = pathNode["filter"].toString();

  QPushButton* browseButton = new QPushButton("Browse");
  horizontalLayout->addWidget(browseButton);
  QObject::connect(browseButton, &QPushButton::clicked, [type, pathField, filter]() {
    // Determine the directory we should open the file browser at.
    QString browseDir;
    if (!pathField->text().isEmpty()) {
      QFileInfo currentValue = QFileInfo(pathField->text());
      auto dir = currentValue.dir();
      if (dir.exists()) {
        browseDir = dir.absolutePath();
      }
    }

    // Now open the appropriate dialog to browse for a file or directory.
    QString path;
    if (type == "file") {
      path = QFileDialog::getOpenFileName(tomviz::mainWidget(), "Select File",
                                          browseDir, filter);
    } else if (type == "save_file") {
      path = QFileDialog::getSaveFileName(tomviz::mainWidget(), "Save File Path",
                                          browseDir, filter);
    } else {
      path = QFileDialog::getExistingDirectory(tomviz::mainWidget(),
                                               "Select Directory", browseDir);
    }

    // If a path was selected update the line edit.
    if (!path.isNull()) {
      pathField->setText(path);
    }
  });
}

void addStringWidget(QGridLayout* layout, int row, QJsonObject& pathNode)
{
  QHBoxLayout* horizontalLayout = new QHBoxLayout;
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  QWidget* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  QJsonValueRef typeValue = pathNode["type"];
  if (typeValue.isUndefined()) {
    QJsonDocument document(pathNode);
    qWarning() << QString("Parameter %1 has no type. Skipping.")
                    .arg(document.toJson().data());
    return;
  }
  QString type = typeValue.toString();

  QJsonValueRef nameValue = pathNode["name"];
  if (nameValue.isUndefined()) {
    QJsonDocument document(pathNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QJsonValueRef labelValue = pathNode["label"];
  QLabel* label = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    label->setText(labelValue.toString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  QLineEdit* stringField = new QLineEdit();
  stringField->setProperty("type", type);
  // Tag the line edit with the type, so we can distinguish it from other line
  // edit uses ( such as in a QSpinBox )
  stringField->setProperty("type", type);
  stringField->setObjectName(nameValue.toString());
  stringField->setMinimumWidth(500);
  label->setBuddy(stringField);
  horizontalLayout->addWidget(stringField);

  QJsonValueRef defaultNode = pathNode["default"];
  if (!defaultNode.isUndefined() && defaultNode.isString()) {
    auto defaultValue = getAs<QString>(defaultNode);
    stringField->setText(defaultValue);
  }
}

void addDatasetWidget(QGridLayout* layout, int row, QJsonObject& parameterNode)
{
  QJsonValueRef nameValue = parameterNode["name"];
  QJsonValueRef labelValue = parameterNode["label"];
  auto defaultId = parameterNode.value("default").toString();

  if (nameValue.isUndefined()) {
    QJsonDocument document(parameterNode);
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(document.toJson().data());
    return;
  }

  QLabel* labelWidget = new QLabel(nameValue.toString());
  if (!labelValue.isUndefined()) {
    labelWidget->setText(labelValue.toString());
  }
  layout->addWidget(labelWidget, row, 0, 1, 1);

  QComboBox* comboBox = new QComboBox();
  comboBox->setObjectName(nameValue.toString());
  labelWidget->setBuddy(comboBox);
  QStringList addedLabels;
  auto dataSources =
    tomviz::ModuleManager::instance().allDataSourcesDepthFirst();
  auto labels = tomviz::ModuleManager::createUniqueLabels(dataSources);
  auto defaultIndex = -1;
  for (int i = 0; i < dataSources.size(); ++i) {
    auto* dataSource = dataSources[i];
    if (dataSource->id() == defaultId) {
      defaultIndex = i;
    }

    auto label = labels[i];

    QVariant data;
    data.setValue(dataSource);
    comboBox->addItem(label, data);
    addedLabels.append(label);
  }

  if (defaultIndex != -1) {
    comboBox->setCurrentIndex(defaultIndex);
  }

  layout->addWidget(comboBox, row, 1, 1, 1);
}

static const QStringList PATH_TYPES = { "file", "save_file", "directory" };

} // end anonymous namespace

namespace tomviz {

bool setupEnableTriggerAbstract(QWidget* refWidget, QWidget* widget,
                                const QString& comparator,
                                const QVariant& compareValue,
                                bool visibility);

InterfaceBuilder::InterfaceBuilder(QObject* parentObject, DataSource* ds)
  : QObject(parentObject), m_dataSource(ds)
{}

void InterfaceBuilder::setJSONDescription(const QString& description)
{
  setJSONDescription(QJsonDocument::fromJson(description.toLatin1()));
}

void InterfaceBuilder::setJSONDescription(const QJsonDocument& description)
{
  if (!description.isObject()) {
    qCritical() << "Failed to parse operator JSON";
    qCritical() << m_json;
    m_json = QJsonDocument();
  } else {
    m_json = description;
  }
}

QLayout* InterfaceBuilder::buildParameterInterface(QGridLayout* layout,
                                                   QJsonArray& parameters,
                                                   const QString& tag) const
{
  QJsonObject::size_type numParameters = parameters.size();
  for (QJsonObject::size_type i = 0; i < numParameters; ++i) {
    QJsonValueRef parameterNode = parameters[i];
    QJsonObject parameterObject = parameterNode.toObject();

    QString tagValue = parameterObject["tag"].toString("");
    if (tagValue != tag) {
      // The tag doesn't match, skip over this one.
      continue;
    }

    QJsonValueRef typeValue = parameterObject["type"];

    if (typeValue.isUndefined()) {
      qWarning() << tr("Parameter has no type entry");
      continue;
    }

    QString typeString = typeValue.toString();

    // See if we have a parameter value that we need to set the default
    // to.
    QJsonValueRef nameValue = parameterObject["name"];
    if (!nameValue.isUndefined()) {
      QString parameterName = nameValue.toString();
      if (m_parameterValues.contains(parameterName)) {
        QVariant parameterValue = m_parameterValues[parameterName];
        if (parameterValue.canConvert<DataSource*>()) {
          // Save the id of the pointer so we can store it in a QJsonValue
          auto* ds = parameterValue.value<DataSource*>();
          parameterValue.setValue(ds->id());
        }

        parameterObject["default"] = QJsonValue::fromVariant(parameterValue);
      }
    }

    if (typeString == "bool") {
      addBoolWidget(layout, i + 1, parameterObject);
    } else if (typeString == "int") {
      addNumericWidget<int>(layout, i + 1, parameterObject, m_dataSource);
    } else if (typeString == "double") {
      addNumericWidget<double>(layout, i + 1, parameterObject, m_dataSource);
    } else if (typeString == "enumeration") {
      addEnumerationWidget(layout, i + 1, parameterObject);
    } else if (typeString == "xyz_header") {
      addXYZHeaderWidget(layout, i + 1, parameterObject);
    } else if (PATH_TYPES.contains(typeString)) {
      addPathWidget(layout, i + 1, parameterObject);
    } else if (typeString == "string") {
      addStringWidget(layout, i + 1, parameterObject);
    } else if (typeString == "dataset") {
      addDatasetWidget(layout, i + 1, parameterObject);
    }
  }

  setupEnableAndVisibleStates(layout->parentWidget(), parameters);

  return layout;
}

void InterfaceBuilder::setupEnableAndVisibleStates(
  const QObject* parent,
  QJsonArray& parameters) const
{
  setupEnableStates(parent, parameters, true);
  setupEnableStates(parent, parameters, false);
}

void InterfaceBuilder::setupEnableStates(const QObject* parent,
                                         QJsonArray& parameters,
                                         bool visible) const
{
  static const QStringList validComparators = {
    "==", "!=", ">", ">=", "<", "<="
  };

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
      qCritical() << text << "parameters must have a name. Ignoring...";
      continue;
    }
    auto* widget = parent->findChild<QWidget*>(widgetName);
    if (!widget) {
      qCritical() << "Failed to find widget with name:" << widgetName;
      continue;
    }

    auto split = enableIfValue.simplified().split(" ");
    if (split.size() != 3) {
      qCritical() << "Invalid" << text << "string:" << enableIfValue;
      continue;
    }

    auto refWidgetName = split[0];
    auto comparator = split[1];
    auto compareValue = split[2];
    auto* refWidget = parent->findChild<QWidget*>(refWidgetName);

    if (!refWidget) {
      qCritical() << "Invalid widget name in" << text << "string:" << enableIfValue;
      continue;
    }

    if (!validComparators.contains(comparator)) {
      qCritical() << "Invalid comparator in" << text << "string:" << enableIfValue;
      continue;
    }

    if (!setupEnableTriggerAbstract(refWidget, widget, comparator,
                                    compareValue, visible)) {
      qCritical() << "Failed to set up" << text << "trigger for" << widgetName;
    }
  }
}

QLayout* InterfaceBuilder::buildInterface() const
{
  QWidget* widget = new QWidget;
  widget->setProperty("isRootInterfaceWidget", true);

  QVBoxLayout* verticalLayout = new QVBoxLayout;
  verticalLayout->addWidget(widget);
  verticalLayout->addStretch();

  QGridLayout* layout = new QGridLayout;
  widget->setLayout(layout);

  if (!m_json.isObject()) {
    return layout;
  }
  QJsonObject root = m_json.object();

  QLabel* descriptionLabel = new QLabel("No description provided in JSON");
  descriptionLabel->setWordWrap(true);
  descriptionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  QJsonValueRef descriptionValue = root["description"];
  if (!descriptionValue.isUndefined()) {
    descriptionLabel->setText(descriptionValue.toString());
  }
  verticalLayout->insertWidget(0, descriptionLabel);

  // Get the label for the operator
  QString operatorLabel;
  QJsonValueRef labelNode = root["label"];
  if (!labelNode.isUndefined()) {
    operatorLabel = QString(labelNode.toString());
  }

  // Get the parameters for the operator
  QJsonValueRef parametersNode = root["parameters"];
  if (parametersNode.isUndefined()) {
    return layout;
  }
  auto parameters = parametersNode.toArray();
  buildParameterInterface(layout, parameters);

  return verticalLayout;
}

static bool setWidgetValue(QObject* o, const QVariant& v)
{
  // Returns true if the widget type was found, false otherwise.

  if (auto cb = qobject_cast<QCheckBox*>(o)) {
    cb->setChecked(v.toBool());
  } else if (auto sb = qobject_cast<tomviz::SpinBox*>(o)) {
    sb->setValue(v.toInt());
  } else if (auto dsb = qobject_cast<tomviz::DoubleSpinBox*>(o)) {
    dsb->setValue(v.toDouble());
  } else if (auto combo = qobject_cast<QComboBox*>(o)) {
    // Check if the variant is a data source
    if (v.canConvert<DataSource*>()) {
      // Find the item with this data, and set it
      auto* ds = v.value<DataSource*>();
      bool found = false;
      for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).value<DataSource*>() == ds) {
          found = true;
          combo->setCurrentIndex(i);
          break;
        }
      }
      if (!found) {
        qDebug() << "Warning: could not find combo box for data source: "
                 << ds->label() << "(" << ds << ")";
      }
    } else {
      // Assume it is a string
      combo->setCurrentText(v.toString());
    }
  } else if (auto le = qobject_cast<QLineEdit*>(o)) {
    le->setText(v.toString());
  } else {
    return false;
  }

  return true;
}

void InterfaceBuilder::setParameterValues(QMap<QString, QVariant> values)
{
  m_parameterValues = values;
}

void InterfaceBuilder::updateWidgetValues(const QObject* parent)
{
  static const QStringList skipTypes = {
    "QLabel",
  };

  for (auto* child : parent->findChildren<QWidget*>()) {
    if (skipTypes.contains(child->metaObject()->className())) {
      // Skip this type...
      continue;
    }

    const auto& name = child->objectName();
    if (!m_parameterValues.contains(name)) {
      continue;
    }

    if (!setWidgetValue(child, m_parameterValues[name])) {
      qDebug() << "Failed to set value for child: " << child;
    }
  }
}

QVariantMap InterfaceBuilder::parameterValues(const QObject* parent)
{
  QVariantMap map;

  // Iterate over all children, taking the value of the named widgets
  // and stuffing them into the map.pathField->setProperty("type", type);
  QList<QCheckBox*> checkBoxes = parent->findChildren<QCheckBox*>();
  for (int i = 0; i < checkBoxes.size(); ++i) {
    map[checkBoxes[i]->objectName()] =
      (checkBoxes[i]->checkState() == Qt::Checked);
  }

  QList<tomviz::SpinBox*> spinBoxes = parent->findChildren<tomviz::SpinBox*>();
  for (int i = 0; i < spinBoxes.size(); ++i) {
    map[spinBoxes[i]->objectName()] = spinBoxes[i]->value();
  }

  QList<tomviz::DoubleSpinBox*> doubleSpinBoxes =
    parent->findChildren<tomviz::DoubleSpinBox*>();
  for (int i = 0; i < doubleSpinBoxes.size(); ++i) {
    map[doubleSpinBoxes[i]->objectName()] = doubleSpinBoxes[i]->value();
  }

  QList<QComboBox*> comboBoxes = parent->findChildren<QComboBox*>();
  for (int i = 0; i < comboBoxes.size(); ++i) {
    int currentIndex = comboBoxes[i]->currentIndex();
    map[comboBoxes[i]->objectName()] = comboBoxes[i]->itemData(currentIndex);
  }

  // Assemble multi-component properties into single properties in the map.
  QMap<QString, QVariant>::iterator iter = map.begin();
  while (iter != map.end()) {
    QString name = iter.key();
    QVariant value = iter.value();
    int poundIndex = name.indexOf(tr("#"));
    if (poundIndex >= 0) {
      QString indexString = name.mid(poundIndex + 1);

      // Keep the part of the name to the left of the '#'
      name = name.left(poundIndex);

      QList<QVariant> valueList;
      QMap<QString, QVariant>::iterator findIter = map.find(name);
      if (findIter != map.end()) {
        valueList = map[name].toList();
      }

      // The QMap keeps entries sorted by lexicographic order, so we
      // can just append to the list and the elements will be inserted
      // in the correct order.
      valueList.append(value);
      map[name] = valueList;

      // Delete the individual component map entry. Doing so increments the
      // iterator.
      iter = map.erase(iter);
    } else {
      // Single-element parameter, nothing to do
      ++iter;
    }
  }

  // QLineEdit's ( currently 'file', 'save_file', and 'directory' types ).
  QList<QLineEdit*> lineEdits = parent->findChildren<QLineEdit*>();
  for (int i = 0; i < lineEdits.size(); ++i) {
    auto lineEdit = lineEdits[i];
    QVariant type = lineEdit->property("type");
    bool canConvertTypeToString = QMetaType::canConvert(type.metaType(), QMetaType(QMetaType::QString));
    if (canConvertTypeToString && PATH_TYPES.contains(type.toString())) {
      map[lineEdit->objectName()] = lineEdit->text();
    } else if (canConvertTypeToString && type.toString() == "string") {
      map[lineEdit->objectName()] = lineEdit->text();
    }
  }

  return map;
}

QWidget* findRootInterfaceWidget(const QWidget* widget)
{
  auto* parent = widget->parent();
  while (parent and not parent->property("isRootInterfaceWidget").toBool()) {
    parent = parent->parent();
  }

  if (parent and parent->property("isRootInterfaceWidget").toBool()) {
    return qobject_cast<QWidget*>(parent);
  }

  return nullptr;
}

QLabel* findLabelForWidget(const QWidget* widget)
{
  // We use the buddy system to keep track of which label is for
  // which widget.
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
  if (isWidgetNumeric(widget) || isWidgetType<QLineEdit>(widget)) {
    // These types actually want the parent widget instead, because there
    // is some parent widget holding everything (spinboxes, path button, etc.).
    widget = widget->parentWidget();
    if (!widget) {
      // This hopefully should't happen.
      return;
    }
  }

  // First, set the property on the widget
  widget->setProperty(property, value);

  // Next, see if we can find the label corresponding to this widget, and
  // set the property there as well.
  auto* label = findLabelForWidget(widget);
  if (label) {
    label->setProperty(property, value);
  }
}

template <typename T>
bool compareGeneric(T value, T ref, const QString& comparator)
{
  // The generic one, for bools and strings, can only do `==` and `!=`.
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
  // This is for ints and floats. We can do inequality comparisons.
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
  if constexpr (std::is_same_v<T, QComboBox> || std::is_same_v<T, QLineEdit>) {
    QString ref = compareValue.toString();
    if (ref.startsWith("'") || ref.startsWith("\"")) {
      // Remove the first and last characters
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

template <typename T>
bool setupEnableTrigger(T* refWidget, QWidget* widget,
                        const QString& comparator, const QVariant& compareValue,
                        const char* property)
{
  // Set up the callback function
  auto func = [=](){
    auto result = compare(refWidget, compareValue, comparator);
    setWidgetProperty(widget, property, result);
  };
  // Make the connection
  widget->connect(refWidget, changedSignal<T>(), widget, func);

  // Trigger the update one time, since defaults are already set.
  func();

  return true;
}

bool setupEnableTriggerAbstract(QWidget* refWidget, QWidget* widget,
                                const QString& comparator,
                                const QVariant& compareValue,
                                bool visibility)
{
  const char* property = visibility ? "visible" : "enabled";
  if (isWidgetType<QSpinBox>(refWidget)) {
    return setupEnableTrigger(qobject_cast<QSpinBox*>(refWidget), widget,
                              comparator, compareValue, property);
  } else if (isWidgetType<QDoubleSpinBox>(refWidget)) {
    return setupEnableTrigger(qobject_cast<QDoubleSpinBox*>(refWidget), widget,
                              comparator, compareValue, property);
  } else if (isWidgetType<QCheckBox>(refWidget)) {
    return setupEnableTrigger(qobject_cast<QCheckBox*>(refWidget), widget,
                              comparator, compareValue, property);
  } else if (isWidgetType<QComboBox>(refWidget)) {
    return setupEnableTrigger(qobject_cast<QComboBox*>(refWidget), widget,
                              comparator, compareValue, property);
  } else if (isWidgetType<QLineEdit>(refWidget)) {
    return setupEnableTrigger(qobject_cast<QLineEdit*>(refWidget), widget,
                              comparator, compareValue, property);
  }

  qCritical() << "Unhandled widget type for object: "
              << refWidget->objectName();
  return false;
}

} // namespace tomviz

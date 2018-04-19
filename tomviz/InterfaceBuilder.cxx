/******************************************************************************

  This source file is part of the tomviz project.

  Copyright Kitware, Inc.

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

******************************************************************************/
#include "InterfaceBuilder.h"

#include "DataSource.h"
#include "DoubleSpinBox.h"
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
  }
  spinBox->setMinimum(rangeMin);
  spinBox->setMaximum(rangeMax);
  spinBox->setValue(defaultValue);
  return spinBox;
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
  layout->addWidget(checkBox, row, 1, 1, 1);
}

template <typename T>
void addNumericWidget(QGridLayout* layout, int row, QJsonObject& parameterNode,
                      tomviz::DataSource* dataSource)
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

  int defaultOption = 0;
  QJsonValueRef defaultNode = parameterNode["default"];
  if (!defaultNode.isUndefined() && isType<int>(defaultNode)) {
    defaultOption = getAs<int>(defaultNode);
  }

  QComboBox* comboBox = new QComboBox();
  comboBox->setObjectName(nameValue.toString());
  QJsonValueRef optionsNode = parameterNode["options"];
  if (!optionsNode.isUndefined()) {
    QJsonArray optionsArray = optionsNode.toArray();
    for (QJsonObject::size_type i = 0; i < optionsArray.size(); ++i) {
      QJsonObject optionNode = optionsArray[i].toObject();
      QString optionName = optionNode.keys()[0];
      QJsonValueRef optionValueNode = optionNode[optionName];
      int optionValue = 0;
      if (isType<int>(optionValueNode)) {
        optionValue = optionValueNode.toInt();
      } else {
        qWarning() << "Option value is not an int. Skipping";
        continue;
      }
      comboBox->addItem(optionName, optionValue);
    }
  }

  comboBox->setCurrentIndex(defaultOption);

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
  horizontalLayout->addWidget(pathField);

  QPushButton* browseButton = new QPushButton("Browse");
  horizontalLayout->addWidget(browseButton);
  QObject::connect(browseButton, &QPushButton::clicked, [type, pathField]() {

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
      path = QFileDialog::getOpenFileName(tomviz::mainWidget(),
                                          "Select File", browseDir);
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
  horizontalLayout->addWidget(stringField);

  QJsonValueRef defaultNode = pathNode["default"];
  if (!defaultNode.isUndefined() && defaultNode.isString()) {
    auto defaultValue = getAs<QString>(defaultNode);
    stringField->setText(defaultValue);
  }
}

} // end anonymous namespace

namespace tomviz {

InterfaceBuilder::InterfaceBuilder(QObject* parentObject, DataSource* ds)
  : QObject(parentObject), m_dataSource(ds)
{
}

void InterfaceBuilder::setJSONDescription(const QString& description)
{
  this->setJSONDescription(QJsonDocument::fromJson(description.toLatin1()));
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
                                                   QJsonArray& parameters) const
{

  QJsonObject::size_type numParameters = parameters.size();
  for (QJsonObject::size_type i = 0; i < numParameters; ++i) {
    QJsonValueRef parameterNode = parameters[i];
    QJsonObject parameterObject = parameterNode.toObject();
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
        parameterObject["default"] = QJsonValue::fromVariant(parameterValue);
      }
    }

    if (typeString == "bool") {
      addBoolWidget(layout, i + 1, parameterObject);
    } else if (typeString == "int") {
      addNumericWidget<int>(layout, i + 1, parameterObject, this->m_dataSource);
    } else if (typeString == "double") {
      addNumericWidget<double>(layout, i + 1, parameterObject,
                               this->m_dataSource);
    } else if (typeString == "enumeration") {
      addEnumerationWidget(layout, i + 1, parameterObject);
    } else if (typeString == "xyz_header") {
      addXYZHeaderWidget(layout, i + 1, parameterObject);
    } else if (typeString == "file" || typeString == "directory") {
      addPathWidget(layout, i + 1, parameterObject);
    } else if (typeString == "string") {
      addStringWidget(layout, i + 1, parameterObject);
    }
  }

  return layout;
}

QLayout* InterfaceBuilder::buildInterface() const
{
  QWidget* widget = new QWidget;

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
  this->buildParameterInterface(layout, parameters);

  return verticalLayout;
}

void InterfaceBuilder::setParameterValues(QMap<QString, QVariant> values)
{
  m_parameterValues = values;
}

QVariantMap InterfaceBuilder::parameterValues(const QObject* parent)
{
  QVariantMap map;
  qDebug() << parent->children().size();

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

  // QLineEdit's ( currently 'file' and 'directory' types ).
  QStringList pathTypes = { "file", "directory" };
  QList<QLineEdit*> lineEdits = parent->findChildren<QLineEdit*>();
  for (int i = 0; i < lineEdits.size(); ++i) {
    auto lineEdit = lineEdits[i];
    QVariant type = lineEdit->property("type");

    if (type.canConvert(QMetaType::QString) &&
        pathTypes.contains(type.toString())) {
      map[lineEdit->objectName()] = lineEdit->text();
    } else if (type.canConvert(QMetaType::QString) &&
               type.toString() == "string") {
      map[lineEdit->objectName()] = lineEdit->text();
    }
  }

  return map;
}

} // namespace tomviz

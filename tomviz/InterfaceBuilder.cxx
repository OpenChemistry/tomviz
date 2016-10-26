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

#include "DoubleSpinBox.h"
#include "SpinBox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
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

// Templated generation of numeric editing widgets.
template <typename T>
QWidget* getNumericWidget(T, T, T)
{
  return nullptr;
}

template <>
QWidget* getNumericWidget(int defaultValue, int rangeMin, int rangeMax)
{
  tomviz::SpinBox* spinBox = new tomviz::SpinBox();
  spinBox->setSingleStep(1);
  spinBox->setMinimum(rangeMin);
  spinBox->setMaximum(rangeMax);
  spinBox->setValue(defaultValue);
  return spinBox;
}

template <>
QWidget* getNumericWidget(double defaultValue, double rangeMin, double rangeMax)
{
  tomviz::DoubleSpinBox* spinBox = new tomviz::DoubleSpinBox();
  spinBox->setSingleStep(0.5);
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
void addNumericWidget(QGridLayout* layout, int row, QJsonObject& parameterNode)
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
  }

  std::vector<T> minValues(defaultValues.size(), std::numeric_limits<T>::min());
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

    QWidget* spinBox =
      getNumericWidget(defaultValues[i], minValues[i], maxValues[i]);
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

} // end anonymous namespace

namespace tomviz {

InterfaceBuilder::InterfaceBuilder(QObject* parentObject)
  : Superclass(parentObject)
{
}

InterfaceBuilder::~InterfaceBuilder()
{
}

void InterfaceBuilder::setJSONDescription(const QString& description)
{
  m_json = description;
}

const QString& InterfaceBuilder::JSONDescription() const
{
  return m_json;
}

QLayout* InterfaceBuilder::buildInterface() const
{
  QGridLayout* layout = new QGridLayout;

  QJsonDocument document = QJsonDocument::fromJson(m_json.toLatin1());
  if (!document.isObject()) {
    qCritical() << "Failed to parse operator JSON";
    qCritical() << m_json;
    return layout;
  }
  QJsonObject root = document.object();

  QLabel* descriptionLabel = new QLabel("No description provided in JSON");
  QJsonValueRef descriptionValue = root["description"];
  if (!descriptionValue.isUndefined()) {
    descriptionLabel->setText(descriptionValue.toString());
  }
  layout->addWidget(descriptionLabel, 0, 0, 1, 2);

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

  QJsonArray parametersArray = parametersNode.toArray();
  QJsonObject::size_type numParameters = parametersArray.size();
  for (QJsonObject::size_type i = 0; i < numParameters; ++i) {
    QJsonValueRef parameterNode = parametersArray[i];
    QJsonObject parameterObject = parameterNode.toObject();
    QJsonValueRef typeValue = parameterObject["type"];

    if (typeValue.isUndefined()) {
      qWarning() << tr("Parameter has no type entry");
      continue;
    }

    QString typeString = typeValue.toString();
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
    }
  }

  return layout;
}

} // namespace tomviz

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
#include <QLabel>
#include <QSpinBox>
#include <QWidget>

// Has to be included before vtk_jsoncpp.h
// vtk_jsoncpp.h needs VTK_BUILD_SHARED_LIBS to be set correctly in order to
// define the export macros for MSVC right. Otherwise you don't get the
// dllimport on all the jsoncpp symbols. This was causing duplicate
// symbol errors.
#include "vtkConfigure.h"

#include "vtk_jsoncpp.h"

namespace {

// Templated query of Json::Value type
template <typename T>
bool isType(const Json::Value&)
{
  return false;
}

template <>
bool isType<int>(const Json::Value& value)
{
  return value.isInt();
}

template <>
bool isType<double>(const Json::Value& value)
{
  return value.isDouble();
}

// Templated accessor of Json::Value value
template <typename T>
T getAs(const Json::Value&)
{
  return 0;
}

template <>
int getAs(const Json::Value& value)
{
  int iValue = 0;
  try {
    iValue = value.asInt();
  } catch (...) {
    qCritical() << "Could not get int from Json::Value";
  }
  return iValue;
}

template <>
double getAs(const Json::Value& value)
{
  double dValue = 0.0;
  try {
    dValue = value.asDouble();
  } catch (...) {
    qCritical() << "Could not get double from Json::Value";
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

void addBoolWidget(QGridLayout* layout, int row,
                   const Json::Value& parameterNode)
{
  Json::Value nameValue = parameterNode["name"];
  Json::Value labelValue = parameterNode["label"];

  if (nameValue.isNull()) {
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(parameterNode.toStyledString().c_str());
    return;
  }

  QLabel* label = new QLabel(nameValue.asCString());
  if (!labelValue.isNull()) {
    label->setText(labelValue.asCString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  bool defaultValue = false;
  if (parameterNode.isMember("default")) {
    Json::Value defaultNode = parameterNode["default"];
    if (defaultNode.isBool()) {
      defaultValue = defaultNode.asBool();
    }
  }
  QCheckBox* checkBox = new QCheckBox();
  checkBox->setObjectName(nameValue.asCString());
  checkBox->setCheckState(defaultValue ? Qt::Checked : Qt::Unchecked);
  layout->addWidget(checkBox, row, 1, 1, 1);
}

template <typename T>
void addNumericWidget(QGridLayout* layout, int row,
                      const Json::Value& parameterNode)
{
  Json::Value nameValue = parameterNode["name"];
  Json::Value labelValue = parameterNode["label"];

  if (nameValue.isNull()) {
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(parameterNode.toStyledString().c_str());
    return;
  }

  QLabel* label = new QLabel(nameValue.asCString());
  if (!labelValue.isNull()) {
    label->setText(labelValue.asCString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  std::vector<T> defaultValues;
  if (parameterNode.isMember("default")) {
    Json::Value defaultNode = parameterNode["default"];
    if (isType<T>(defaultNode)) {
      defaultValues.push_back(getAs<T>(defaultNode));
    } else if (defaultNode.isArray()) {
      for (Json::Value::ArrayIndex i = 0; i < defaultNode.size(); ++i) {
        defaultValues.push_back(getAs<T>(defaultNode[i]));
      }
    }
  }

  std::vector<T> minValues(defaultValues.size(), std::numeric_limits<T>::min());
  Json::Value minNode = parameterNode["minimum"];
  if (isType<T>(minNode)) {
    minValues[0] = getAs<T>(minNode);
  } else if (minNode.isArray()) {
    for (Json::Value::ArrayIndex i = 0; i < minNode.size(); ++i) {
      minValues[i] = getAs<T>(minNode[i]);
    }
  }

  std::vector<T> maxValues(defaultValues.size(), std::numeric_limits<T>::max());
  Json::Value maxNode = parameterNode["maximum"];
  if (isType<T>(maxNode)) {
    maxValues[0] = getAs<T>(maxNode);
  } else if (maxNode.isArray()) {
    for (Json::Value::ArrayIndex i = 0; i < maxNode.size(); ++i) {
      maxValues[i] = getAs<T>(maxNode[i]);
    }
  }

  QHBoxLayout* horizontalLayout = new QHBoxLayout();
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  QWidget* horizontalWidget = new QWidget;
  horizontalWidget->setLayout(horizontalLayout);
  layout->addWidget(horizontalWidget, row, 1, 1, 1);

  for (size_t i = 0; i < defaultValues.size(); ++i) {
    QString name(nameValue.asCString());
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
                          const Json::Value& parameterNode)
{
  Json::Value nameValue = parameterNode["name"];
  Json::Value labelValue = parameterNode["label"];

  if (nameValue.isNull()) {
    qWarning() << QString("Parameter %1 has no name. Skipping.")
                    .arg(parameterNode.toStyledString().c_str());
    return;
  }

  QLabel* label = new QLabel(nameValue.asCString());
  if (!labelValue.isNull()) {
    label->setText(labelValue.asCString());
  }
  layout->addWidget(label, row, 0, 1, 1);

  int defaultOption = 0;
  Json::Value defaultNode = parameterNode["default"];
  if (!defaultNode.isNull() && defaultNode.isInt()) {
    defaultOption = defaultNode.asInt();
  }

  QComboBox* comboBox = new QComboBox();
  comboBox->setObjectName(nameValue.asCString());
  Json::Value optionsNode = parameterNode["options"];
  if (!optionsNode.isNull()) {
    for (Json::Value::ArrayIndex i = 0; i < optionsNode.size(); ++i) {
      std::string optionName = optionsNode[i].getMemberNames()[0];
      Json::Value optionValueNode = optionsNode[i][optionName];
      int optionValue = 0;
      if (optionValueNode.isInt()) {
        optionValue = optionValueNode.asInt();
      } else {
        qWarning() << "Option value is not an int. Skipping";
        continue;
      }
      comboBox->addItem(optionName.c_str(), optionValue);
    }
  }

  comboBox->setCurrentIndex(defaultOption);

  layout->addWidget(comboBox, row, 1, 1, 1);
}

void addXYZHeaderWidget(QGridLayout* layout, int row, const Json::Value&)
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

  Json::Value root;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(m_json.toLatin1().data(), root);
  if (!parsingSuccessful) {
    qCritical() << "Failed to parse operator JSON";
    qCritical() << m_json;
    return layout;
  }

  QLabel* descriptionLabel = new QLabel("No description provided in JSON");
  Json::Value descriptionValue = root["description"];
  if (!descriptionValue.isNull()) {
    descriptionLabel->setText(descriptionValue.asCString());
  }
  layout->addWidget(descriptionLabel, 0, 0, 1, 2);

  // Get the label for the operator
  QString operatorLabel;
  Json::Value labelNode = root["label"];
  if (!labelNode.isNull()) {
    operatorLabel = QString(labelNode.asCString());
  }

  // Get the parameters for the operator
  Json::Value parametersNode = root["parameters"];
  if (parametersNode.isNull()) {
    return layout;
  }

  Json::Value::ArrayIndex numParameters = parametersNode.size();
  for (Json::Value::ArrayIndex i = 0; i < numParameters; ++i) {
    Json::Value parameterNode = parametersNode[i];
    Json::Value typeValue = parameterNode["type"];

    if (typeValue.isNull()) {
      qWarning() << tr("Parameter has no type entry");
      continue;
    }

    std::string typeString = typeValue.asString();
    if (typeString == "bool") {
      addBoolWidget(layout, i + 1, parameterNode);
    } else if (typeString == "int") {
      addNumericWidget<int>(layout, i + 1, parameterNode);
    } else if (typeString == "double") {
      addNumericWidget<double>(layout, i + 1, parameterNode);
    } else if (typeString == "enumeration") {
      addEnumerationWidget(layout, i + 1, parameterNode);
    } else if (typeString == "xyz_header") {
      addXYZHeaderWidget(layout, i + 1, parameterNode);
    }
  }

  return layout;
}

} // namespace tomviz

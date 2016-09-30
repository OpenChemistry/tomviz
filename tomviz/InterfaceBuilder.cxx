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
#include <QDebug>
#include <QLabel>
#include <QSpinBox>
#include <QWidget>

#include "vtk_jsoncpp.h"

namespace {

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

void addIntWidget(QGridLayout* layout, int row,
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

  int defaultValue = 0;
  if (parameterNode.isMember("default")) {
    Json::Value defaultNode = parameterNode["default"];
    if (defaultNode.isInt()) {
      defaultValue = defaultNode.asInt();
    }
  }
  tomviz::SpinBox* spinBox = new tomviz::SpinBox();
  spinBox->setObjectName(nameValue.asCString());
  spinBox->setSingleStep(1);
  spinBox->setMinimum(std::numeric_limits<int>::min());
  spinBox->setMaximum(std::numeric_limits<int>::max());
  spinBox->setValue(defaultValue);

  if (parameterNode.isMember("minimum")) {
    int minimum = parameterNode["minimum"].asInt();
    spinBox->setMinimum(minimum);
  }
  if (parameterNode.isMember("maximum")) {
    int maximum = parameterNode["maximum"].asInt();
    spinBox->setMaximum(maximum);
  }
  layout->addWidget(spinBox, row, 1, 1, 1);
}

void addDoubleWidget(QGridLayout* layout, int row,
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

  double defaultValue = 0.0;
  if (parameterNode.isMember("default")) {
    Json::Value defaultNode = parameterNode["default"];
    if (defaultNode.isDouble()) {
      defaultValue = defaultNode.asDouble();
    }
  }
  tomviz::DoubleSpinBox* spinBox = new tomviz::DoubleSpinBox();
  spinBox->setObjectName(nameValue.asCString());
  spinBox->setSingleStep(0.5);
  spinBox->setMinimum(std::numeric_limits<double>::min());
  spinBox->setMaximum(std::numeric_limits<double>::max());
  spinBox->setValue(defaultValue);

  if (parameterNode.isMember("minimum")) {
    int minimum = parameterNode["minimum"].asInt();
    spinBox->setMinimum(minimum);
  }
  if (parameterNode.isMember("maximum")) {
    int maximum = parameterNode["maximum"].asInt();
    spinBox->setMaximum(maximum);
  }
  layout->addWidget(spinBox, row, 1, 1, 1);
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
      addIntWidget(layout, i + 1, parameterNode);
    } else if (typeString == "double") {
      addDoubleWidget(layout, i + 1, parameterNode);
    }
  }

  return layout;
}

} // namespace tomviz

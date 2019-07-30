/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizAddPythonTransformReaction_h
#define tomvizAddPythonTransformReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;
class OperatorPython;

class AddPythonTransformReaction : public pqReaction
{
  Q_OBJECT

public:
  AddPythonTransformReaction(QAction* parent, const QString& label,
                             const QString& source,
                             bool requiresTiltSeries = false,
                             bool requiresVolume = false,
                             bool requiresFib = false,
                             const QString& json = QString());

  OperatorPython* addExpression(DataSource* source = nullptr);

  void setInteractive(bool isInteractive) { interactive = isInteractive; }

  // Used to create a python operator with a JSON string describing its
  // arguments. Do not use this version of the function to create an operator
  // with arguments but no JSON description.
  static void addPythonOperator(DataSource* source, const QString& scriptLabel,
                                const QString& scriptBaseString,
                                const QMap<QString, QVariant> arguments,
                                const QString& jsonString);

  // Used to create a python operator with no arguments.
  static void addPythonOperator(DataSource* source, const QString& scriptLabel,
                                const QString& scriptBaseString)
  {
    addPythonOperator(source, scriptLabel, scriptBaseString,
                      QMap<QString, QVariant>(), "");
  }

  // Used to create a python operator with arguments but no JSON describing
  // them. The typeInfo map must be a map of argument name -> type.  Without
  // this the operator will fail to load from a state file.
  static void addPythonOperator(DataSource* source, const QString& scriptLabel,
                                const QString& scriptBaseString,
                                const QMap<QString, QVariant> arguments,
                                const QMap<QString, QString> typeInfo);

protected:
  void updateEnableState() override;

  void onTriggered() override { addExpression(); }

private slots:
  void addExpressionFromNonModalDialog();

private:
  Q_DISABLE_COPY(AddPythonTransformReaction)

  QString jsonSource;
  QString scriptLabel;
  QString scriptSource;

  bool interactive;
  bool requiresTiltSeries;
  bool requiresVolume;
  bool requiresFib;
  bool externalCompatible = true;
};
} // namespace tomviz

#endif

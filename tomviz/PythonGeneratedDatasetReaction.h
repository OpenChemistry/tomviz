/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPythonGeneratedDatasetReaction_h
#define tomvizPythonGeneratedDatasetReaction_h

#include <pqReaction.h>

namespace tomviz {
class DataSource;

class PythonGeneratedDatasetReaction : public pqReaction
{
  Q_OBJECT

public:
  PythonGeneratedDatasetReaction(QAction* parent, const QString& label,
                                 const QString& source);

  void addDataset();

  static DataSource* createDataSource(const QJsonObject& sourceInformation);

protected:
  void onTriggered() override { this->addDataset(); }

private:
  Q_DISABLE_COPY(PythonGeneratedDatasetReaction)

  QString m_scriptLabel;
  QString m_scriptSource;
};
} // namespace tomviz

#endif

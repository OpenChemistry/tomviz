/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLoadDataReaction_h
#define tomvizLoadDataReaction_h

#include <pqReaction.h>

#include <QJsonObject>

#include "ImageStackModel.h"

class vtkImageData;
class vtkSMProxy;

namespace tomviz {
class DataSource;
class MoleculeSource;

class PythonReaderFactory;

/// LoadDataReaction handles the "Load Data" action in tomviz. On trigger,
/// this will open the data file and necessary subsequent actions, including:
/// \li make the data source "active".
///
class LoadDataReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadDataReaction(QAction* parentAction);
  ~LoadDataReaction() override;

  static QList<DataSource*> loadData();

  /// Convenience method, adds defaultModules, addToRecent, and child to the
  /// JSON object before passing it to the loadData methods.
  static DataSource* loadData(const QString& fileName, bool defaultModules,
                              bool addToRecent, bool child,
                              const QJsonObject& options = QJsonObject());

  /// Load a data file from the specified location, options can be used to pass
  /// additional parameters to the method, such as defaultModules, addToRecent,
  /// and child, or pvXML to pass to the ParaView reader.
  static DataSource* loadData(const QString& fileName,
                              const QJsonObject& options = QJsonObject());

  /// Load data files from the specified locations, options can be used to pass
  /// additional parameters to the method, such as defaultModules, addToRecent,
  /// and child, or pvXML to pass to the ParaView reader.
  static DataSource* loadData(const QStringList& fileNames,
                              const QJsonObject& options = QJsonObject());

  static QList<MoleculeSource*> loadMolecule(
    const QStringList& fileNames, const QJsonObject& options = QJsonObject());
  static MoleculeSource* loadMolecule(
    const QString& fileName, const QJsonObject& options = QJsonObject());

  /// Handle creation of a new data source.
  static void dataSourceAdded(DataSource* dataSource,
                              bool defaultModules = true, bool child = false);

protected:
  /// Create a raw data source from the reader.
  static DataSource* createDataSource(vtkSMProxy* reader,
                                      bool defaultModules = true,
                                      bool child = false);

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(LoadDataReaction)

  static void addDefaultModules(DataSource* dataSource);
  static QJsonObject readerProperties(vtkSMProxy* reader);
  static void setFileNameProperties(const QJsonObject& props,
                                    vtkSMProxy* reader);
};
} // namespace tomviz

#endif

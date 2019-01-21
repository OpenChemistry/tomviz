/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLoadStackReaction_h
#define tomvizLoadStackReaction_h

#include <pqReaction.h>

#include "ImageStackModel.h"

namespace tomviz {
class DataSource;
class ImageStackDialog;

/// LoadStackReaction handles the "Load Stack" action in tomviz. On trigger,
/// this will open a dialog where the user can drag-n-drop or open multiple
/// files
/// or a folder. After selecting the files in the stack, options will be
/// presented
/// to include or exclude each file, and to label which type of stack it is
///
class LoadStackReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadStackReaction(QAction* parentAction);
  ~LoadStackReaction() override;

  static DataSource* loadData();
  static DataSource* loadData(QStringList fileNames);
  static DataSource* loadData(QString directory);
  static QList<ImageInfo> loadTiffStack(const QStringList& fileNames);

protected:
  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Q_DISABLE_COPY(LoadStackReaction)

  static DataSource* execStackDialog(ImageStackDialog& dialog);
  static QStringList summaryToFileNames(const QList<ImageInfo>& summary);
};
} // namespace tomviz

#endif

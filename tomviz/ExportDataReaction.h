/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizExportDataReaction_h
#define tomvizExportDataReaction_h

#include <pqReaction.h>

class vtkImageData;
class vtkSMSourceProxy;

namespace tomviz {
class Module;

/// ExportDataReaction handles the "Export as ..." action in tomviz. On trigger,
/// this will save the data from the active module (or the module that is set)
/// to a file
class ExportDataReaction : public pqReaction
{
  Q_OBJECT

public:
  ExportDataReaction(QAction* parentAction, Module* module = nullptr);

  /// Save the file
  bool exportData(const QString& filename);
  bool exportColoredSlice(vtkImageData* imageData, vtkSMSourceProxy* writer,
                          const QString& filename);

protected:
  /// Called when the data changes to enable/disable the menu item
  void updateEnableState() override;

  /// Called when the action is triggered.
  void onTriggered() override;

private:
  Module* m_module;

  Q_DISABLE_COPY(ExportDataReaction)
};
} // namespace tomviz
#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelineModuleMenu_h
#define tomvizPipelineModuleMenu_h

#include <QObject>
#include <QPointer>

#include "pipeline/PortType.h"

class QAction;
class QMenu;
class QToolBar;

namespace tomviz {

namespace pipeline {
class Pipeline;
class SinkNode;
class LegacyModuleSink;
class OutputPort;
} // namespace pipeline

/// PipelineModuleMenu manages the Visualization menu and toolbar for the
/// new pipeline infrastructure. It creates sink nodes instead of old Modules.
class PipelineModuleMenu : public QObject
{
  Q_OBJECT

public:
  PipelineModuleMenu(QToolBar* toolBar, QMenu* parentMenu,
                     QObject* parent = nullptr);
  ~PipelineModuleMenu() override;

  static QList<QString> sinkTypes();
  static QIcon sinkIcon(const QString& type);
  static pipeline::PortTypes sinkAcceptedTypes(const QString& type);
  /// Whether @a type is the right visualization to offer for @a port.
  /// Narrower than isPortTypeCompatible(): a sink can accept a base
  /// type but still not be what the menu should suggest for one of its
  /// subtypes, and Label Map is offered for what a port's data holds
  /// rather than only for how the port is typed.
  static bool sinkSuitsPort(const QString& type,
                            pipeline::OutputPort* port);
  static pipeline::LegacyModuleSink* createSink(const QString& type);

protected:
  bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
  void updateActions();
  void updateEnableState();
  void triggered(QAction* action);

private:
  Q_DISABLE_COPY(PipelineModuleMenu)
  QPointer<QMenu> m_menu;
  QPointer<QToolBar> m_toolBar;
  bool m_ctrlHeld = false;
};

} // namespace tomviz

#endif

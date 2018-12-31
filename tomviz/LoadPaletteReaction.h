/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizLoadPaletteReaction_h
#define tomvizLoadPaletteReaction_h

#include <pqReaction.h>

#include <QPointer>
#include <QStringList>

class QAction;

namespace tomviz {

class LoadPaletteReaction : public pqReaction
{
  Q_OBJECT

public:
  LoadPaletteReaction(QAction* parentObject = nullptr);
  ~LoadPaletteReaction() override;

private slots:
  void populateMenu();
  void actionTriggered(QAction* actn);

private:
  Q_DISABLE_COPY(LoadPaletteReaction)
  QPointer<QMenu> m_menu;
  QStringList m_paletteWhiteList;
};
} // namespace tomviz
#endif

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "AnimationHelperDialog.h"
#include "ui_AnimationHelperDialog.h"

#include "ActiveObjects.h"
#include "Utilities.h"

#include <pqRenderView.h>

#include <QPointer>
#include <QPushButton>

namespace tomviz {

class AnimationHelperDialog::Internal : public QObject
{
public:
  Ui::AnimationHelperDialog ui;
  QPointer<AnimationHelperDialog> parent;

  Internal(AnimationHelperDialog* p) : QObject(p), parent(p)
  {
    // Must call setupUi() before using p in any way
    ui.setupUi(p);

    setupConnections();
  }

  void setupConnections()
  {
    connect(ui.buttonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked, this, &Internal::apply);
  }

  void apply()
  {
    auto* renderView = ActiveObjects::instance().activePqRenderView();

    if (ui.createCameraOrbit->isChecked()) {
      // Remove all previous camera cues, and create the orbit
      clearCameraCues(renderView->getRenderViewProxy());
      createCameraOrbit(renderView->getRenderViewProxy());
    }
  }

}; // end class Internal

AnimationHelperDialog::AnimationHelperDialog(QWidget* parent)
  : QDialog(parent), m_internal(new Internal(this))
{
}

AnimationHelperDialog::~AnimationHelperDialog() = default;

} // namespace tomviz

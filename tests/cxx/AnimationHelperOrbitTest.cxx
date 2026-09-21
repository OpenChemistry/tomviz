/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTest>

#include <pqActiveObjects.h>
#include <pqAnimationManager.h>
#include <pqAnimationScene.h>
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqSMAdaptor.h>
#include <pqServerResource.h>

#include <vtkCamera.h>
#include <vtkSMPropertyHelper.h>
#include <vtkSMRenderViewProxy.h>

#include <algorithm>
#include <functional>
#include <vtkSMProxy.h>

#include "ActiveObjects.h"
#include "AnimationHelperDialog.h"
#include "AnimationSerializer.h"
#include "animations/AnimationSceneGuard.h"
#include "animations/CameraViewpoints.h"
#include "animations/ModuleAnimation.h"
#include "pipeline/Pipeline.h"

using namespace tomviz;

namespace {

pqAnimationScene* scene()
{
  return pqPVApplicationCore::instance()->animationManager()->getActiveScene();
}

/// Counts ticks, and presses a button on one of them.
struct TickProbe : ModuleAnimation
{
  TickProbe() : ModuleAnimation(nullptr) {}
  int ticks = 0;
  int pressAt = -1;
  QPushButton* button = nullptr;
  std::function<void()> action;
  pqRenderView* view = nullptr;
  double lowestX = 0.0;
  double highestX = 0.0;
  void reset()
  {
    ticks = 0;
    pressAt = -1;
    button = nullptr;
    action = nullptr;
    lowestX = 1e30;
    highestX = -1e30;
  }
  double sweep() const { return highestX - lowestX; }
  void onTimeChanged() override
  {
    if (++ticks == pressAt) {
      if (button) {
        button->click();
      }
      if (action) {
        action();
      }
    }
    if (view) {
      double position[3];
      view->getRenderViewProxy()->GetActiveCamera()->GetPosition(position);
      lowestX = std::min(lowestX, position[0]);
      highestX = std::max(highestX, position[0]);
    }
  }
};

void play(int frames)
{
  auto* proxy = scene()->getProxy();
  pqSMAdaptor::setEnumerationProperty(proxy->GetProperty("PlayMode"),
                                      "Sequence");
  pqSMAdaptor::setElementProperty(proxy->GetProperty("NumberOfFrames"),
                                  frames);
  proxy->UpdateVTKObjects();
  proxy->InvokeCommand("Play");
}

} // namespace

/// Drives the Animation Helper's camera tab the way a user would, with
/// no display: the orbit controls, the always-on flight and Clear All.
class AnimationHelperOrbitTest : public QObject
{
  Q_OBJECT

private slots:
  void initTestCase()
  {
    auto* server = pqActiveObjects::instance().activeServer();
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    m_view = qobject_cast<pqRenderView*>(
      builder->createView(pqRenderView::renderViewType(), server));
    QVERIFY(m_view);
    pqActiveObjects::instance().setActiveView(m_view);
    ActiveObjects::instance().setPipeline(&m_pipeline);
  }

  void init()
  {
    CameraViewpoints::instance().clear();
    m_dialog = new AnimationHelperDialog(nullptr);
    m_add = m_dialog->findChild<QPushButton*>("addViewpoint");
    m_clear = m_dialog->findChild<QPushButton*>("clearAllAnimations");
    m_remove = m_dialog->findChild<QPushButton*>("removeViewpoint");
    m_export = m_dialog->findChild<QPushButton*>("exportMovie");
    m_list = m_dialog->findChild<QListWidget*>("viewpointList");
    m_orbit = m_dialog->findChild<QCheckBox*>("viewpointOrbit");
    m_turns = m_dialog->findChild<QSpinBox*>("orbitTurns");
    m_direction = m_dialog->findChild<QComboBox*>("orbitDirection");
    m_duration = m_dialog->findChild<QDoubleSpinBox*>("orbitDuration");
    QVERIFY(m_add && m_clear && m_remove && m_export && m_list && m_orbit &&
            m_turns && m_direction && m_duration);
  }

  void cleanup()
  {
    delete m_dialog;
    m_dialog = nullptr;
    CameraViewpoints::instance().clear();
  }

  void orbitControlsFollowTheSelectedViewpoint()
  {
    auto& viewpoints = CameraViewpoints::instance();

    // Nothing selected: the box is there but off, the details hidden
    QVERIFY(!m_orbit->isEnabled());
    QVERIFY(!m_orbit->isChecked());
    QVERIFY(m_turns->isHidden());

    // One plain viewpoint is not a path, so nothing flies yet
    m_add->click();
    QCOMPARE(viewpoints.size(), 1);
    QCOMPARE(m_list->currentRow(), 0);
    QVERIFY(!viewpoints.isPath());
    QVERIFY(!viewpoints.isFlying());
    QVERIFY(m_orbit->isEnabled());
    QVERIFY(!m_orbit->isChecked());
    QVERIFY(m_turns->isHidden());
    QVERIFY(m_direction->isHidden());
    QVERIFY(m_duration->isHidden());

    // Ticking Add orbit makes the lone viewpoint a path, arms the flight
    // and reveals the details with their defaults
    m_orbit->setChecked(true);
    QCOMPARE(viewpoints.at(0).orbitTurns, 1);
    QCOMPARE(viewpoints.at(0).orbitDuration, 1.0);
    QVERIFY(viewpoints.isPath());
    QVERIFY(viewpoints.isFlying());
    QVERIFY(!m_turns->isHidden());
    QVERIFY(!m_direction->isHidden());
    QVERIFY(!m_duration->isHidden());
    QCOMPARE(m_turns->value(), 1);
    QCOMPARE(m_direction->currentIndex(), 0);
    QVERIFY(m_export->isEnabled());

    // The details write straight through to the viewpoint
    m_turns->setValue(3);
    QCOMPARE(viewpoints.at(0).orbitTurns, 3);
    m_direction->setCurrentIndex(1);
    QCOMPARE(viewpoints.at(0).orbitTurns, -3);
    m_duration->setValue(2.5);
    QCOMPARE(viewpoints.at(0).orbitDuration, 2.5);
    // And survive the list being rebuilt around them
    QCOMPARE(m_turns->value(), 3);
    QCOMPARE(m_direction->currentIndex(), 1);
    QCOMPARE(m_duration->value(), 2.5);

    // Unticking takes the orbit away, and with it the path and the flight
    m_orbit->setChecked(false);
    QCOMPARE(viewpoints.at(0).orbitTurns, 0);
    QVERIFY(!viewpoints.isPath());
    QVERIFY(!viewpoints.isFlying());
    QVERIFY(m_turns->isHidden());

    // Back on it starts afresh, one turn counterclockwise; set it up
    // again, then a second viewpoint: the controls show each row's own
    // orbit, and the second viewpoint alone keeps the path flying
    m_orbit->setChecked(true);
    QCOMPARE(viewpoints.at(0).orbitTurns, 1);
    QCOMPARE(viewpoints.at(0).orbitDuration, 2.5);
    m_turns->setValue(3);
    m_direction->setCurrentIndex(1);
    QCOMPARE(viewpoints.at(0).orbitTurns, -3);
    m_add->click();
    QCOMPARE(viewpoints.size(), 2);
    QCOMPARE(m_list->currentRow(), 1);
    QVERIFY(!m_orbit->isChecked());
    QVERIFY(m_turns->isHidden());
    QVERIFY(viewpoints.isFlying());
    m_list->setCurrentRow(0);
    QVERIFY(m_orbit->isChecked());
    QCOMPARE(m_turns->value(), 3);
    QCOMPARE(m_direction->currentIndex(), 1);
    QCOMPARE(m_duration->value(), 2.5);
    m_list->setCurrentRow(1);
    m_orbit->setChecked(false);
    QVERIFY2(viewpoints.at(0).orbitTurns == -3, "the other row was edited");
    QVERIFY(viewpoints.isFlying());
    m_orbit->setChecked(false);
    m_list->setCurrentRow(0);
    m_orbit->setChecked(false);
    QCOMPARE(viewpoints.at(0).orbitTurns, 0);
    QVERIFY2(viewpoints.isFlying(), "two viewpoints are still a path");

    // Clear All removes the viewpoints and stops the camera with them
    m_clear->click();
    QCOMPARE(viewpoints.size(), 0);
    QVERIFY(!viewpoints.isFlying());
    QVERIFY(!m_export->isEnabled());
    QVERIFY(!m_orbit->isEnabled());
  }

  // An edit made while the animation plays stops it after that tick and
  // rewinds it, so the next Play shows the edit from the top.
  void editingWhilePlayingStopsAndRewinds()
  {
    auto& viewpoints = CameraViewpoints::instance();
    m_add->click();
    QCOMPARE(viewpoints.size(), 1);

    TickProbe probe;
    probe.pressAt = 20;
    probe.button = m_add;
    play(100);
    QCOMPARE(viewpoints.size(), 2);
    QVERIFY2(probe.ticks < 30,
             qPrintable(QString("ran on for %1 ticks").arg(probe.ticks)));

    // The rewind is deferred until the play loop has unwound
    QTest::qWait(50);
    auto* proxy = scene()->getProxy();
    proxy->UpdatePropertyInformation();
    QCOMPARE(vtkSMPropertyHelper(proxy, "AnimationTime").GetAsDouble(), 0.0);

    // From the top, all the way through
    probe.pressAt = -1;
    probe.ticks = 0;
    proxy->InvokeCommand("Play");
    QVERIFY2(probe.ticks >= 98,
             qPrintable(QString("only %1 ticks").arg(probe.ticks)));

    // A paused animation is not disturbed by an edit
    m_orbit->setChecked(true);
    QTest::qWait(50);
    proxy->UpdatePropertyInformation();
    QVERIFY(vtkSMPropertyHelper(proxy, "AnimationTime").GetAsDouble() > 0.5);
  }

  // The opening Camera Orbit removed while it plays: the playback stops,
  // and a path built afterwards flies as usual.
  void removingTheOnlyViewpointWhilePlayingLeavesTheNextPathWorking()
  {
    auto& viewpoints = CameraViewpoints::instance();
    auto* camera = m_view->getRenderViewProxy()->GetActiveCamera();
    camera->SetPosition(0, 0, 10);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    m_add->click();
    m_orbit->setChecked(true);
    QVERIFY(viewpoints.isFlying());

    TickProbe probe;
    probe.view = m_view;
    probe.pressAt = 20;
    probe.button = m_remove;
    m_list->setCurrentRow(0);
    play(100);
    QCOMPARE(viewpoints.size(), 0);
    QVERIFY(!viewpoints.isFlying());
    QVERIFY(probe.ticks < 30);
    QTest::qWait(50);

    // Nothing to fly now: Play runs the clock and leaves the camera
    // where the interrupted orbit put it
    probe.reset();
    scene()->getProxy()->InvokeCommand("Play");
    QVERIFY(probe.ticks >= 98);
    QVERIFY(probe.sweep() < 1e-9);

    // A new orbit at the current view flies again, all the way round
    camera->SetPosition(0, 0, 10);
    m_add->click();
    m_orbit->setChecked(true);
    QVERIFY(viewpoints.isFlying());
    probe.reset();
    scene()->getProxy()->InvokeCommand("Play");
    QVERIFY2(probe.ticks >= 98, qPrintable(QString("%1 ticks").arg(probe.ticks)));
    QVERIFY2(probe.highestX > 8.0, "never swung out to the right");
    QVERIFY2(probe.lowestX < -8.0, "never swung out to the left");
  }

  // Every edit a user can make from the dialog, made from inside a
  // tick of a running playback. After each: the playback has stopped
  // and rewound, the flight matches the path, and a fresh Play runs
  // through and moves the camera whenever there is a path to fly.
  void everyEditDuringPlaybackLeavesTheAnimationWorking()
  {
    auto& viewpoints = CameraViewpoints::instance();
    auto* camera = m_view->getRenderViewProxy()->GetActiveCamera();
    auto* proxy = scene()->getProxy();
    auto* update = m_dialog->findChild<QPushButton*>("updateViewpoint");
    auto* duration = m_dialog->findChild<QDoubleSpinBox*>("segmentDuration");
    auto* frames = m_dialog->findChild<QSpinBox*>("numberOfFrames");
    QVERIFY(update && duration && frames);

    camera->SetPosition(0, 0, 10);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    m_add->click();
    m_orbit->setChecked(true);
    camera->SetPosition(20, 0, 10);
    m_add->click();
    QCOMPARE(viewpoints.size(), 2);

    TickProbe probe;
    probe.view = m_view;
    struct Edit
    {
      const char* name;
      std::function<void()> act;
      bool rewinds = true;
    };
    const QList<Edit> edits = {
      { "update from view", [&]() { m_list->setCurrentRow(1); update->click(); } },
      { "reorder", [&]() { viewpoints.move(0, 1); } },
      // Go To stops the playback but keeps the camera where it was sent
      { "go to", [&]() { emit m_list->itemDoubleClicked(m_list->item(0)); }, false },
      // Rename applies on the next event-loop turn, once the list widget
      // is done with its own edit
      { "rename", [&]() { m_list->item(0)->setText("Hero shot"); } },
      { "leg duration", [&]() { m_list->setCurrentRow(0); duration->setValue(2.0); } },
      { "orbit turns", [&]() { m_list->setCurrentRow(0); m_orbit->setChecked(true); m_turns->setValue(2); } },
      { "orbit off", [&]() { m_list->setCurrentRow(0); m_orbit->setChecked(false); } },
      { "frame count", [&]() { frames->setValue(80); } },
      { "remove one", [&]() { m_list->setCurrentRow(1); m_remove->click(); } },
      { "clear all", [&]() { m_clear->click(); } },
    };
    for (const auto& edit : edits) {
      const int before = viewpoints.size();
      probe.reset();
      probe.pressAt = 10;
      probe.action = edit.act;
      play(60);
      QVERIFY2(probe.ticks < 25,
               qPrintable(QString("%1: ran on for %2 ticks").arg(edit.name).arg(probe.ticks)));
      QTest::qWait(50);
      proxy->UpdatePropertyInformation();
      const double time = vtkSMPropertyHelper(proxy, "AnimationTime").GetAsDouble();
      QVERIFY2(edit.rewinds ? time == 0.0 : time > 0.0,
               qPrintable(QString("%1: time %2").arg(edit.name).arg(time)));
      QVERIFY2(viewpoints.isFlying() == viewpoints.isPath(),
               qPrintable(QString("%1: flight does not match the path").arg(edit.name)));

      probe.reset();
      proxy->InvokeCommand("Play");
      // A resumed playback picks up after the tick that stopped it
      const int expected = vtkSMPropertyHelper(proxy, "NumberOfFrames").GetAsInt() -
                           (edit.rewinds ? 0 : 10);
      QVERIFY2(probe.ticks >= expected - 2,
               qPrintable(QString("%1: only %2 of %3 ticks").arg(edit.name).arg(probe.ticks).arg(expected)));
      if (viewpoints.isPath()) {
        QVERIFY2(probe.sweep() > 1.0,
                 qPrintable(QString("%1: the camera did not move").arg(edit.name)));
      } else {
        QVERIFY2(probe.sweep() < 1e-9,
                 qPrintable(QString("%1: nothing to fly, yet the camera moved").arg(edit.name)));
      }
      Q_UNUSED(before);
    }
    QCOMPARE(viewpoints.size(), 0);
  }

  // A rename commits after the list widget has finished its own edit,
  // and the list and the model agree afterwards.
  void renamingAViewpointRebuildsTheListSafely()
  {
    auto& viewpoints = CameraViewpoints::instance();
    m_add->click();
    m_add->click();
    QCOMPARE(viewpoints.size(), 2);
    m_list->setCurrentRow(1);
    auto* item = m_list->item(1);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_list->editItem(item);
    item->setText("Detail");
    m_list->closePersistentEditor(item);
    // Not yet: the widget is still the owner of that edit
    QCOMPARE(viewpoints.at(1).name, QString("Viewpoint 2"));
    QTest::qWait(20);
    QCOMPARE(viewpoints.at(1).name, QString("Detail"));
    QCOMPARE(m_list->count(), 2);
    QCOMPARE(m_list->item(1)->text(), QString("Detail"));
    QCOMPARE(m_list->currentRow(), 1);

    // An empty name is rejected and the old one comes back
    m_list->item(1)->setText("   ");
    QTest::qWait(20);
    QCOMPARE(viewpoints.at(1).name, QString("Detail"));
    QCOMPARE(m_list->item(1)->text(), QString("Detail"));
  }

  // The view the flight was armed in goes away; the next render view
  // picks the path up, with no help from the dialog.
  void aNewRenderViewPicksUpTheFlight()
  {
    auto& viewpoints = CameraViewpoints::instance();
    auto* camera = m_view->getRenderViewProxy()->GetActiveCamera();
    camera->SetPosition(0, 0, 10);
    camera->SetFocalPoint(0, 0, 0);
    camera->SetViewUp(0, 1, 0);
    m_add->click();
    m_orbit->setChecked(true);
    QVERIFY(viewpoints.isFlying());

    auto* server = pqActiveObjects::instance().activeServer();
    auto* builder = pqApplicationCore::instance()->getObjectBuilder();
    builder->destroy(m_view);
    QTest::qWait(50);
    QVERIFY(!viewpoints.isFlying());

    m_view = qobject_cast<pqRenderView*>(
      builder->createView(pqRenderView::renderViewType(), server));
    QVERIFY(m_view);
    pqActiveObjects::instance().setActiveView(m_view);
    QTest::qWait(50);
    QVERIFY2(viewpoints.isFlying(), "the new view did not pick up the path");

    TickProbe probe;
    probe.view = m_view;
    probe.reset();
    play(60);
    QVERIFY(probe.ticks >= 58);
    QVERIFY2(probe.sweep() > 8.0, "the camera did not fly in the new view");
  }

  void legacyOrbitStateBecomesAnOrbitingViewpoint()
  {
    auto& viewpoints = CameraViewpoints::instance();
    QJsonObject animation;
    animation["viewpoints"] = QJsonArray();
    animation["cameraOrbit"] = true;
    QJsonObject doc;
    doc["animation"] = animation;

    AnimationSerializer::restore(doc, &m_pipeline);
    QCOMPARE(viewpoints.size(), 1);
    QCOMPARE(viewpoints.at(0).orbitTurns, 1);
    QCOMPARE(viewpoints.at(0).name, QString("Camera Orbit"));
    QVERIFY(viewpoints.isFlying());
    QCOMPARE(m_list->count(), 1);
    QCOMPARE(m_list->currentRow(), -1);

    // A file with a path of its own is left alone
    QJsonObject saved;
    AnimationSerializer::save(saved);
    QVERIFY(!saved["animation"].toObject().contains("cameraOrbit"));
    auto again = saved["animation"].toObject();
    again["cameraOrbit"] = true;
    doc["animation"] = again;
    AnimationSerializer::restore(doc, &m_pipeline);
    QCOMPARE(viewpoints.size(), 1);
    QVERIFY(viewpoints.isFlying());

    // And one with no path at all flies nothing
    QJsonObject empty;
    empty["viewpoints"] = QJsonArray();
    doc["animation"] = empty;
    AnimationSerializer::restore(doc, &m_pipeline);
    QCOMPARE(viewpoints.size(), 0);
    QVERIFY(!viewpoints.isFlying());
  }

private:
  pipeline::Pipeline m_pipeline;
  pqRenderView* m_view = nullptr;
  AnimationHelperDialog* m_dialog = nullptr;
  QPushButton* m_add = nullptr;
  QPushButton* m_clear = nullptr;
  QPushButton* m_remove = nullptr;
  QPushButton* m_export = nullptr;
  QListWidget* m_list = nullptr;
  QCheckBox* m_orbit = nullptr;
  QSpinBox* m_turns = nullptr;
  QComboBox* m_direction = nullptr;
  QDoubleSpinBox* m_duration = nullptr;
};

int main(int argc, char** argv)
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  pqPVApplicationCore appCore(argc, argv);
  pqApplicationCore::instance()->getObjectBuilder()->createServer(
    pqServerResource("builtin:"));
  // Installed by the main window in the application
  AnimationSceneGuard guard;
  AnimationHelperOrbitTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "AnimationHelperOrbitTest.moc"

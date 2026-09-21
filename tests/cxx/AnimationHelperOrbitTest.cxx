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
#include <pqApplicationCore.h>
#include <pqObjectBuilder.h>
#include <pqPVApplicationCore.h>
#include <pqRenderView.h>
#include <pqServerResource.h>

#include "ActiveObjects.h"
#include "AnimationHelperDialog.h"
#include "AnimationSerializer.h"
#include "animations/CameraViewpoints.h"
#include "pipeline/Pipeline.h"

using namespace tomviz;

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
    m_export = m_dialog->findChild<QPushButton*>("exportMovie");
    m_list = m_dialog->findChild<QListWidget*>("viewpointList");
    m_orbit = m_dialog->findChild<QCheckBox*>("viewpointOrbit");
    m_turns = m_dialog->findChild<QSpinBox*>("orbitTurns");
    m_direction = m_dialog->findChild<QComboBox*>("orbitDirection");
    m_duration = m_dialog->findChild<QDoubleSpinBox*>("orbitDuration");
    QVERIFY(m_add && m_clear && m_export && m_list && m_orbit && m_turns &&
            m_direction && m_duration);
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
  AnimationHelperOrbitTest tc;
  return QTest::qExec(&tc, argc, argv);
}

#include "AnimationHelperOrbitTest.moc"

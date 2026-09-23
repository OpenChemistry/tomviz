/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "OperatorSearchDialog.h"

#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QTest>

#include "TomvizTest.h"

using tomviz::OperatorSearchDialog;

namespace {

class OperatorSearchDialogTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    tomviz_test::ensureQApp();
    dialog = new OperatorSearchDialog();
    blur = new QAction("Blur", dialog);
    broken = new QAction("Broken", dialog);
    broken->setEnabled(false);
    crop = new QAction("Crop", dialog);
    dialog->addOperatorAction(blur, "Custom Transforms", "Blurs things");
    dialog->addOperatorAction(broken, "Custom Transforms");
    dialog->addOperatorAction(crop, "Data Transforms");
    QObject::connect(blur, &QAction::triggered, [this]() { ++blurs; });
  }

  void TearDown() override { delete dialog; }

  // Showing the dialog rebuilds its lists from the registered entries.
  QStringList listed() const
  {
    QStringList names;
    for (auto* list : dialog->findChildren<QListWidget*>()) {
      for (int i = 0; i < list->count(); ++i) {
        names.append(list->item(i)->text());
      }
    }
    names.sort();
    return names;
  }

  OperatorSearchDialog* dialog = nullptr;
  QAction* blur = nullptr;
  QAction* broken = nullptr;
  QAction* crop = nullptr;
  int blurs = 0;
};

} // namespace

TEST_F(OperatorSearchDialogTest, lists_registered_actions_by_availability)
{
  dialog->show();
  EXPECT_EQ(listed(), (QStringList{ "Blur", "Broken", "Crop" }));

  // The disabled one sits in the "Unavailable" list, the others don't.
  int withBroken = 0;
  for (auto* list : dialog->findChildren<QListWidget*>()) {
    for (int i = 0; i < list->count(); ++i) {
      if (list->item(i)->text() == "Broken") {
        ++withBroken;
        EXPECT_EQ(list->count(), 1);
      }
    }
  }
  EXPECT_EQ(withBroken, 1);
}

TEST_F(OperatorSearchDialogTest, a_category_can_be_replaced)
{
  // The custom menu rebuilds its actions on every open; the host drops
  // the old entries and registers the new ones.
  dialog->removeCategory("Custom Transforms");
  dialog->show();
  EXPECT_EQ(listed(), QStringList{ "Crop" });

  auto* sharpen = new QAction("Sharpen", dialog);
  dialog->addOperatorAction(sharpen, "Custom Transforms");
  dialog->hide();
  dialog->show();
  EXPECT_EQ(listed(), (QStringList{ "Crop", "Sharpen" }));
}

TEST_F(OperatorSearchDialogTest, a_deleted_action_disappears_without_harm)
{
  delete blur;
  blur = nullptr;
  dialog->show();
  EXPECT_EQ(listed(), (QStringList{ "Broken", "Crop" }));
}

TEST_F(OperatorSearchDialogTest, enter_triggers_the_filtered_match)
{
  dialog->show();
  auto* box = dialog->findChild<QLineEdit*>();
  ASSERT_NE(box, nullptr);
  box->setText("blu");
  EXPECT_EQ(listed(), QStringList{ "Blur" });

  QTest::keyClick(dialog, Qt::Key_Return);
  EXPECT_EQ(blurs, 1);
  EXPECT_FALSE(dialog->isVisible());
}

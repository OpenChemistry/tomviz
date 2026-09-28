/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "CustomOperatorManagerDialog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QToolButton>

#include <algorithm>
#include <memory>
#include <vector>

#include "TomvizTest.h"

using tomviz::CustomOperatorManagerDialog;
using tomviz::OperatorDescription;

namespace {

OperatorDescription describe(const QString& label, const QString& dir,
                             bool userOwned,
                             OperatorDescription::Type type =
                               OperatorDescription::Type::Transform)
{
  OperatorDescription op;
  op.label = label;
  op.pythonPath = dir + "/" + label + ".py";
  op.jsonPath = dir + "/" + label + ".json";
  op.userOwned = userOwned;
  op.type = type;
  return op;
}

// A directory as the dialog displays it: absolute, with the platform's
// separators (and drive letter on Windows).
QString nativeDirectory(const QString& directory)
{
  return QDir::toNativeSeparators(QFileInfo(directory).absoluteFilePath());
}

class CustomOperatorManagerDialogTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    tomviz_test::ensureQApp();
    operators = {
      describe("Blur", "/user", true),
      describe("Noise", "/elsewhere", false, OperatorDescription::Type::Source),
      describe("Broken", "/user", true),
    };
    operators[2].valid = false;
    operators[2].loadError = "boom: line 3";

    // The provider hands out whatever the test has put in `operators`, so
    // a refresh can be exercised without touching the filesystem.
    dialog = std::make_unique<CustomOperatorManagerDialog>(
      [this]() { return operators; });
    QObject::connect(dialog.get(),
                     &CustomOperatorManagerDialog::createRequested,
                     [this]() { ++creates; });
    QObject::connect(dialog.get(), &CustomOperatorManagerDialog::cloneRequested,
                     [this](const OperatorDescription& op) {
                       requested = op.label;
                       ++clones;
                     });
    QObject::connect(dialog.get(), &CustomOperatorManagerDialog::editRequested,
                     [this](const OperatorDescription& op) {
                       requested = op.label;
                       ++edits;
                     });
    QObject::connect(dialog.get(),
                     &CustomOperatorManagerDialog::deleteRequested,
                     [this](const OperatorDescription& op) {
                       requested = op.label;
                       ++deletes;
                     });
    QObject::connect(dialog.get(),
                     &CustomOperatorManagerDialog::openDirectoryRequested,
                     [this](const OperatorDescription& op) {
                       requested = op.label;
                       ++opens;
                     });
    dialog->show();
    settle();
  }

  // Layouts are applied on posted events.
  static void settle() { QCoreApplication::processEvents(); }

  /// The widget of type T named @a objectName on the row for @a label.
  template <typename T>
  T* rowWidget(const char* objectName, const QString& label) const
  {
    for (T* widget : dialog->template findChildren<T*>(objectName)) {
      if (widget->property("operatorLabel").toString() == label) {
        return widget;
      }
    }
    return nullptr;
  }

  QToolButton* button(const char* name, const QString& label) const
  {
    return rowWidget<QToolButton>(name, label);
  }

  QLabel* section(const QString& title) const
  {
    for (QLabel* label :
         dialog->findChildren<QLabel*>("customOperatorSection")) {
      if (label->text() == title) {
        return label;
      }
    }
    return nullptr;
  }

  std::vector<OperatorDescription> operators;
  std::unique_ptr<CustomOperatorManagerDialog> dialog;
  QString requested;
  int creates = 0;
  int clones = 0;
  int edits = 0;
  int deletes = 0;
  int opens = 0;
};

} // namespace

TEST_F(CustomOperatorManagerDialogTest, groups_sources_above_transforms)
{
  auto* sources = section("Sources");
  auto* transforms = section("Transforms");
  auto* noise = rowWidget<QLabel>("customOperatorName", "Noise");
  auto* blur = rowWidget<QLabel>("customOperatorName", "Blur");
  auto* broken = rowWidget<QLabel>("customOperatorName", "Broken");
  ASSERT_NE(sources, nullptr);
  ASSERT_NE(transforms, nullptr);
  ASSERT_NE(noise, nullptr);
  ASSERT_NE(blur, nullptr);
  ASSERT_NE(broken, nullptr);

  // Top to bottom: Sources, Noise, Transforms, Blur, Broken.
  EXPECT_LT(sources->y(), noise->y());
  EXPECT_LT(noise->y(), transforms->y());
  EXPECT_LT(transforms->y(), blur->y());
  EXPECT_LT(blur->y(), broken->y());

  // No table header and no type column: the grouping says it all.
  EXPECT_EQ(dialog->findChildren<QLabel*>("customOperatorSection").size(), 2);
  EXPECT_FALSE(
    dialog->findChild<QLabel*>("customOperatorEmptyHint")->isVisible());
}

TEST_F(CustomOperatorManagerDialogTest, rows_show_path_and_state)
{
  // The directory is shown the way the platform writes it (a drive letter
  // and backslashes on Windows).
  auto* blurPath = rowWidget<QLabel>("customOperatorPath", "Blur");
  ASSERT_NE(blurPath, nullptr);
  EXPECT_EQ(blurPath->toolTip(), nativeDirectory("/user"));
  EXPECT_EQ(blurPath->text(), nativeDirectory("/user"));

  auto* noisePath = rowWidget<QLabel>("customOperatorPath", "Noise");
  ASSERT_NE(noisePath, nullptr);
  EXPECT_EQ(noisePath->toolTip(), nativeDirectory("/elsewhere"));

  // A broken definition is greyed and says why on hover.
  EXPECT_FALSE(rowWidget<QLabel>("customOperatorName", "Broken")->isEnabled());
  auto* status = rowWidget<QLabel>("customOperatorStatus", "Broken");
  ASSERT_NE(status, nullptr);
  EXPECT_TRUE(status->toolTip().contains("boom"));
  EXPECT_EQ(rowWidget<QLabel>("customOperatorStatus", "Blur"), nullptr);
}

TEST_F(CustomOperatorManagerDialogTest, long_paths_are_elided_in_the_middle)
{
  operators = { describe("Deep",
                         "/a/very/long/directory/name/that/keeps/going/on/"
                         "and/on/well/past/any/reasonable/dialog/width/"
                         "until/it/has/to/give",
                         true) };
  dialog->refresh();
  dialog->resize(600, 300);
  settle();

  // Both ends survive, the middle goes, and the tooltip keeps it all.
  auto* path = rowWidget<QLabel>("customOperatorPath", "Deep");
  ASSERT_NE(path, nullptr);
  const QString full = nativeDirectory("/a/very/long/directory");
  const QString shown = path->text();
  EXPECT_TRUE(shown.contains(QChar(0x2026))) << shown.toStdString();
  EXPECT_LT(shown.size(), path->toolTip().size());
  EXPECT_TRUE(shown.startsWith(full.left(4))) << shown.toStdString();
  EXPECT_TRUE(shown.endsWith("give")) << shown.toStdString();
  EXPECT_TRUE(path->toolTip().startsWith(full)) << full.toStdString();
  EXPECT_FALSE(path->toolTip().contains(QChar(0x2026)));
}

TEST_F(CustomOperatorManagerDialogTest, buttons_follow_ownership)
{
  // The user's own files: everything.
  EXPECT_TRUE(button("editCustomOperator", "Blur")->isEnabled());
  EXPECT_TRUE(button("deleteCustomOperator", "Blur")->isEnabled());
  EXPECT_TRUE(button("cloneCustomOperator", "Blur")->isEnabled());

  // From another directory: clone only, and the buttons say why.
  EXPECT_FALSE(button("editCustomOperator", "Noise")->isEnabled());
  EXPECT_FALSE(button("deleteCustomOperator", "Noise")->isEnabled());
  EXPECT_TRUE(button("cloneCustomOperator", "Noise")->isEnabled());
  EXPECT_FALSE(button("editCustomOperator", "Noise")->toolTip().isEmpty());

  // A broken definition in the user's directory can still be fixed,
  // copied or removed.
  EXPECT_TRUE(button("editCustomOperator", "Broken")->isEnabled());
  EXPECT_TRUE(button("deleteCustomOperator", "Broken")->isEnabled());
  EXPECT_TRUE(button("cloneCustomOperator", "Broken")->isEnabled());

  // The folder can always be opened.
  for (const char* label : { "Blur", "Noise", "Broken" }) {
    EXPECT_TRUE(button("openCustomOperatorDirectory", label)->isEnabled());
  }
}

TEST_F(CustomOperatorManagerDialogTest, buttons_are_icons_with_tooltips)
{
  // The icon resources are registered. Whether an SVG renders here
  // depends on the test process finding Qt's SVG plugin, so a button is
  // allowed to fall back to its verb as text; it never comes up blank.
  for (const char* resource :
       { ":/icons/edit.png", ":/icons/clone.svg", ":/icons/folder.svg" }) {
    EXPECT_TRUE(QFile::exists(resource)) << resource;
  }
  for (const char* name : { "editCustomOperator", "deleteCustomOperator",
                            "cloneCustomOperator",
                            "openCustomOperatorDirectory" }) {
    auto* blur = button(name, "Blur");
    ASSERT_NE(blur, nullptr) << name;
    EXPECT_TRUE(!blur->icon().isNull() || !blur->text().isEmpty()) << name;
    EXPECT_FALSE(blur->toolTip().isEmpty()) << name;
  }
  EXPECT_EQ(button("editCustomOperator", "Blur")->toolTip(), "Edit");
  EXPECT_EQ(button("deleteCustomOperator", "Blur")->toolTip(), "Delete");
  EXPECT_EQ(button("cloneCustomOperator", "Blur")->toolTip(), "Clone");
  EXPECT_EQ(button("openCustomOperatorDirectory", "Blur")->toolTip(),
            "Open containing folder");
}

TEST_F(CustomOperatorManagerDialogTest, buttons_request_their_own_row)
{
  dialog->findChild<QPushButton*>("createCustomOperator")->click();
  EXPECT_EQ(creates, 1);

  button("cloneCustomOperator", "Noise")->click();
  EXPECT_EQ(clones, 1);
  EXPECT_EQ(requested, "Noise");

  button("editCustomOperator", "Blur")->click();
  EXPECT_EQ(edits, 1);
  EXPECT_EQ(requested, "Blur");

  button("deleteCustomOperator", "Broken")->click();
  EXPECT_EQ(deletes, 1);
  EXPECT_EQ(requested, "Broken");

  button("openCustomOperatorDirectory", "Noise")->click();
  EXPECT_EQ(opens, 1);
  EXPECT_EQ(requested, "Noise");
  EXPECT_EQ(clones, 1);
  EXPECT_EQ(creates, 1);
}

TEST_F(CustomOperatorManagerDialogTest, a_handler_may_refresh_mid_click)
{
  // Deleting rebuilds the list from inside the click of a button that the
  // rebuild destroys; the request must still carry the right operator.
  QObject::connect(dialog.get(), &CustomOperatorManagerDialog::deleteRequested,
                   [this](const OperatorDescription& op) {
                     operators.erase(
                       std::remove_if(operators.begin(), operators.end(),
                                      [&op](const OperatorDescription& other) {
                                        return other.label == op.label;
                                      }),
                       operators.end());
                     dialog->refresh();
                   });
  button("deleteCustomOperator", "Blur")->click();
  settle();
  EXPECT_EQ(requested, "Blur");
  EXPECT_EQ(button("deleteCustomOperator", "Blur"), nullptr);
  EXPECT_NE(button("deleteCustomOperator", "Broken"), nullptr);
}

TEST_F(CustomOperatorManagerDialogTest, refresh_requeries_the_provider)
{
  operators.erase(operators.begin() + 1);
  dialog->refresh();
  settle();
  EXPECT_EQ(rowWidget<QLabel>("customOperatorName", "Noise"), nullptr);
  EXPECT_EQ(section("Sources"), nullptr);
  EXPECT_NE(section("Transforms"), nullptr);

  // The Refresh button does the same as refresh(); an empty list shows
  // the hint and keeps Create New available.
  operators.clear();
  dialog->findChild<QPushButton*>("refreshCustomOperators")->click();
  settle();
  EXPECT_TRUE(dialog->findChildren<QLabel*>("customOperatorName").isEmpty());
  EXPECT_TRUE(
    dialog->findChild<QLabel*>("customOperatorEmptyHint")->isVisible());
  EXPECT_TRUE(
    dialog->findChild<QPushButton*>("createCustomOperator")->isEnabled());
}

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "sinks/LightingPresetStore.h"

#include <QSignalSpy>

using tomviz::pipeline::LightingPresetStore;
using tomviz::pipeline::UserLightingPreset;

namespace {

// The store is shared by the whole application (memory-only here, since
// there is no application core), so each test starts from an empty one.
class LightingPresetStoreTest : public ::testing::Test
{
protected:
  void SetUp() override { clear(); }
  void TearDown() override { clear(); }

  void clear()
  {
    for (const auto& preset : store().presets()) {
      store().remove(preset.name);
    }
  }

  LightingPresetStore& store() { return LightingPresetStore::instance(); }

  UserLightingPreset preset(const QString& name, double ambient)
  {
    UserLightingPreset p;
    p.name = name;
    p.ambient = ambient;
    return p;
  }
};

} // namespace

TEST_F(LightingPresetStoreTest, RenameKeepsTheValuesUnderTheNewName)
{
  store().save(preset("Warm", 0.4));
  QSignalSpy changed(&store(), &LightingPresetStore::changed);

  EXPECT_TRUE(store().rename("Warm", "Warmer"));
  EXPECT_FALSE(store().contains("Warm"));
  ASSERT_TRUE(store().contains("Warmer"));
  EXPECT_DOUBLE_EQ(store().preset("Warmer").ambient, 0.4);
  EXPECT_EQ(changed.count(), 1);

  // Surrounding whitespace is not part of a name
  EXPECT_TRUE(store().rename("Warmer", "  Warmest "));
  EXPECT_TRUE(store().contains("Warmest"));
}

TEST_F(LightingPresetStoreTest, RenameRefusesBlankTakenAndUnknownNames)
{
  store().save(preset("A", 0.1));
  store().save(preset("B", 0.2));
  QSignalSpy changed(&store(), &LightingPresetStore::changed);

  EXPECT_FALSE(store().rename("A", ""));
  EXPECT_FALSE(store().rename("A", "   "));
  EXPECT_FALSE(store().rename("A", "B"));
  EXPECT_FALSE(store().rename("Nobody", "C"));
  // Renaming to the same name is fine and changes nothing
  EXPECT_TRUE(store().rename("A", "A"));

  EXPECT_EQ(changed.count(), 0);
  EXPECT_TRUE(store().contains("A"));
  EXPECT_TRUE(store().contains("B"));
  EXPECT_DOUBLE_EQ(store().preset("B").ambient, 0.2);
  EXPECT_EQ(store().presets().size(), 2);
}

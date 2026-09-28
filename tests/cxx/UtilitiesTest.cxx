/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkTable.h>

#include "TomvizTest.h"
#include "Utilities.h"

#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QTemporaryDir>

using namespace tomviz;

class UtilitiesTest : public ::testing::Test
{
};

TEST_F(UtilitiesTest, table_to_csv_basic)
{
  vtkNew<vtkTable> table;

  vtkNew<vtkDoubleArray> colX;
  colX->SetName("x");
  colX->SetNumberOfTuples(3);
  colX->SetValue(0, 1.0);
  colX->SetValue(1, 2.0);
  colX->SetValue(2, 3.0);

  vtkNew<vtkDoubleArray> colY;
  colY->SetName("y");
  colY->SetNumberOfTuples(3);
  colY->SetValue(0, 4.0);
  colY->SetValue(1, 5.0);
  colY->SetValue(2, 6.0);

  table->AddColumn(colX);
  table->AddColumn(colY);

  QString csv = tableToCsv(table);
  QStringList lines = csv.split("\n");

  ASSERT_EQ(lines.size(), 4); // header + 3 data rows
  ASSERT_STREQ(lines[0].toLatin1().constData(), "x,y");
  ASSERT_STREQ(lines[1].toLatin1().constData(), "1,4");
  ASSERT_STREQ(lines[2].toLatin1().constData(), "2,5");
  ASSERT_STREQ(lines[3].toLatin1().constData(), "3,6");
}

TEST_F(UtilitiesTest, table_to_csv_multiple_columns)
{
  vtkNew<vtkTable> table;

  const char* names[] = { "a", "b", "c", "d" };
  for (int col = 0; col < 4; ++col) {
    vtkNew<vtkDoubleArray> arr;
    arr->SetName(names[col]);
    arr->SetNumberOfTuples(2);
    arr->SetValue(0, col + 1.0);
    arr->SetValue(1, col + 10.0);
    table->AddColumn(arr);
  }

  QString csv = tableToCsv(table);
  QStringList lines = csv.split("\n");

  ASSERT_EQ(lines.size(), 3); // header + 2 data rows
  ASSERT_STREQ(lines[0].toLatin1().constData(), "a,b,c,d");

  // Verify comma separation in data rows
  QStringList fields = lines[1].split(",");
  ASSERT_EQ(fields.size(), 4);
}

TEST_F(UtilitiesTest, table_to_csv_empty_table)
{
  vtkNew<vtkTable> table;

  vtkNew<vtkDoubleArray> colX;
  colX->SetName("x");
  colX->SetNumberOfTuples(0);

  vtkNew<vtkDoubleArray> colY;
  colY->SetName("y");
  colY->SetNumberOfTuples(0);

  table->AddColumn(colX);
  table->AddColumn(colY);

  QString csv = tableToCsv(table);
  QStringList lines = csv.split("\n");

  ASSERT_EQ(lines.size(), 1); // header only
  ASSERT_STREQ(lines[0].toLatin1().constData(), "x,y");
}

TEST_F(UtilitiesTest, table_to_csv_single_row)
{
  vtkNew<vtkTable> table;

  vtkNew<vtkDoubleArray> colX;
  colX->SetName("x");
  colX->SetNumberOfTuples(1);
  colX->SetValue(0, 42.0);

  vtkNew<vtkDoubleArray> colY;
  colY->SetName("y");
  colY->SetNumberOfTuples(1);
  colY->SetValue(0, 99.0);

  table->AddColumn(colX);
  table->AddColumn(colY);

  QString csv = tableToCsv(table);
  QStringList lines = csv.split("\n");

  ASSERT_EQ(lines.size(), 2); // header + 1 data row
  ASSERT_STREQ(lines[0].toLatin1().constData(), "x,y");
  ASSERT_STREQ(lines[1].toLatin1().constData(), "42,99");
}

namespace {

// Restores an environment variable to whatever the process had.
struct ScopedEnv
{
  explicit ScopedEnv(const char* variable)
    : name(variable), had(qEnvironmentVariableIsSet(variable)),
      old(qgetenv(variable))
  {
  }
  ~ScopedEnv()
  {
    if (had) {
      qputenv(name, old);
    } else {
      qunsetenv(name);
    }
  }
  const char* name;
  bool had;
  QByteArray old;
};

constexpr const char* kCustomTransformsPath = "TOMVIZ_CUSTOM_TRANSFORMS_PATH";
constexpr const char* kUserDirectory = "TOMVIZ_USER_DIRECTORY";

} // namespace

TEST_F(UtilitiesTest, user_data_path_follows_env_override)
{
  ScopedEnv guard(kUserDirectory);
  QTemporaryDir root;
  ASSERT_TRUE(root.isValid());
  // Nested and not yet existing: the override is created like the default.
  const QString wanted = root.path() + "/nested/user-dir";
  qputenv(kUserDirectory, wanted.toLocal8Bit());

  EXPECT_EQ(userDataPath(), QDir::cleanPath(wanted));
  EXPECT_TRUE(QFileInfo(wanted).isDir());
  EXPECT_EQ(userTemplatesPath(), QDir::cleanPath(wanted) + "/templates");
}

TEST_F(UtilitiesTest, user_data_path_defaults_to_tomviz_under_home)
{
  ScopedEnv guard(kUserDirectory);
  qunsetenv(kUserDirectory);

  const QString path = userDataPath();
  EXPECT_TRUE(path.endsWith("/tomviz")) << path.toStdString();
  EXPECT_TRUE(QFileInfo(path).isDir());
}

TEST_F(UtilitiesTest, custom_operator_search_paths_follow_env_override)
{
  ScopedEnv guard(kCustomTransformsPath);
  QTemporaryDir first;
  QTemporaryDir second;
  ASSERT_TRUE(first.isValid() && second.isValid());
  const QString missing = first.path() + "/does-not-exist";

  const QString value = QStringList{ first.path(), missing, second.path() }
                          .join(QDir::listSeparator());
  qputenv(kCustomTransformsPath, value.toLocal8Bit());

  const QStringList paths = customOperatorSearchPaths();
  const QStringList expected{ QDir::cleanPath(first.path()),
                              QDir::cleanPath(second.path()) };
  EXPECT_EQ(paths, expected);
}

TEST_F(UtilitiesTest, custom_operator_search_paths_default_to_existing_dirs)
{
  ScopedEnv guard(kCustomTransformsPath);
  qunsetenv(kCustomTransformsPath);
  QTemporaryDir unrelated;

  const QStringList paths = customOperatorSearchPaths();
  EXPECT_FALSE(paths.contains(QDir::cleanPath(unrelated.path())));
  for (const QString& path : paths) {
    EXPECT_TRUE(QFileInfo(path).isDir()) << path.toStdString();
  }
}

TEST_F(UtilitiesTest, custom_operator_search_paths_keep_the_dot_tomviz_dir)
{
  ScopedEnv guard(kCustomTransformsPath);
  qunsetenv(kCustomTransformsPath);
  const QString home =
    QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first();
  const QString legacy = QDir::cleanPath(QDir(home).filePath(".tomviz"));

  const QStringList paths = customOperatorSearchPaths();
  EXPECT_EQ(paths.contains(legacy), QFileInfo(legacy).isDir())
    << legacy.toStdString();
  // The environment override replaces the default locations entirely.
  QTemporaryDir only;
  qputenv(kCustomTransformsPath, only.path().toLocal8Bit());
  EXPECT_FALSE(customOperatorSearchPaths().contains(legacy));
}

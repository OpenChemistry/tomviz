/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkTable.h>

#include "TomvizTest.h"
#include "Utilities.h"

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

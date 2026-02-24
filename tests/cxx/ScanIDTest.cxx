/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include <vtkDataObject.h>
#include <vtkSmartPointer.h>

#include "DataSource.h"
#include "TomvizTest.h"

using namespace tomviz;

class ScanIDTest : public ::testing::Test
{
protected:
  void SetUp() override { dataObject = vtkSmartPointer<vtkDataObject>::New(); }

  vtkSmartPointer<vtkDataObject> dataObject;
};

TEST_F(ScanIDTest, scan_ids_not_present_by_default)
{
  ASSERT_FALSE(DataSource::hasScanIDs(dataObject));
}

TEST_F(ScanIDTest, set_and_get_scan_ids)
{
  QVector<int> ids = { 1, 2, 3 };
  DataSource::setScanIDs(dataObject, ids);

  ASSERT_TRUE(DataSource::hasScanIDs(dataObject));

  auto retrieved = DataSource::getScanIDs(dataObject);
  ASSERT_EQ(retrieved.size(), 3);
  ASSERT_EQ(retrieved[0], 1);
  ASSERT_EQ(retrieved[1], 2);
  ASSERT_EQ(retrieved[2], 3);
}

TEST_F(ScanIDTest, clear_scan_ids)
{
  QVector<int> ids = { 1, 2, 3 };
  DataSource::setScanIDs(dataObject, ids);
  ASSERT_TRUE(DataSource::hasScanIDs(dataObject));

  DataSource::clearScanIDs(dataObject);
  ASSERT_FALSE(DataSource::hasScanIDs(dataObject));
}

TEST_F(ScanIDTest, empty_scan_ids)
{
  QVector<int> ids;
  DataSource::setScanIDs(dataObject, ids);

  ASSERT_TRUE(DataSource::hasScanIDs(dataObject));
  auto retrieved = DataSource::getScanIDs(dataObject);
  ASSERT_EQ(retrieved.size(), 0);
}

TEST_F(ScanIDTest, large_scan_id_set)
{
  QVector<int> ids;
  for (int i = 0; i < 200; ++i) {
    ids.append(i * 10);
  }
  DataSource::setScanIDs(dataObject, ids);

  ASSERT_TRUE(DataSource::hasScanIDs(dataObject));
  auto retrieved = DataSource::getScanIDs(dataObject);
  ASSERT_EQ(retrieved.size(), 200);
  for (int i = 0; i < 200; ++i) {
    ASSERT_EQ(retrieved[i], i * 10);
  }
}

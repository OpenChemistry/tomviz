/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "LabelSelection.h"

using tomviz::formatLabelList;
using tomviz::parseLabelList;

TEST(LabelSelectionTest, ParsesValuesRangesAndLooseSeparators)
{
  EXPECT_EQ(parseLabelList("3, 5, 10-12"),
            (QVector<double>{ 3, 5, 10, 11, 12 }));
  EXPECT_EQ(parseLabelList("7;1 4\n2"), (QVector<double>{ 1, 2, 4, 7 }));
  // Reversed and overlapping runs, duplicates
  EXPECT_EQ(parseLabelList("5-3, 4, 4"), (QVector<double>{ 3, 4, 5 }));
  EXPECT_EQ(parseLabelList("1 - 3"), (QVector<double>{ 1, 2, 3 }));
  EXPECT_TRUE(parseLabelList("").isEmpty());
  EXPECT_TRUE(parseLabelList(" , ; ").isEmpty());
  // Things that are not labels are skipped rather than fatal
  EXPECT_EQ(parseLabelList("2, two, 3"), (QVector<double>{ 2, 3 }));
}

TEST(LabelSelectionTest, FormatsRunsOfThreeOrMoreAsRanges)
{
  EXPECT_EQ(formatLabelList({ 3, 5, 10, 11, 12 }), "3, 5, 10-12");
  // Two in a row stay separate; order and duplicates are tidied
  EXPECT_EQ(formatLabelList({ 2, 1, 1 }), "1, 2");
  EXPECT_EQ(formatLabelList({ 4, 3, 2, 1, 9 }), "1-4, 9");
  EXPECT_EQ(formatLabelList({}), "");
}

TEST(LabelSelectionTest, FormatAndParseRoundTrip)
{
  const QVector<double> labels{ 1, 2, 3, 7, 20, 21, 22, 23, 40 };
  EXPECT_EQ(parseLabelList(formatLabelList(labels)), labels);
}

/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include <gtest/gtest.h>

#include "TomvizTest.h"
#include "Variant.h"

using namespace tomviz;

class VariantTest : public ::testing::Test
{
};

TEST_F(VariantTest, boolean)
{
  bool testBool = true;
  Variant boo(testBool);

  ASSERT_EQ(boo.toBool(), testBool);

  // Assignment
  Variant assignment(false);
  assignment = boo;
  ASSERT_EQ(assignment.toBool(), testBool);

  // Copy
  Variant copy(boo);
  ASSERT_EQ(copy.toBool(), testBool);
}

TEST_F(VariantTest, integer)
{
  int testInt = 47;
  Variant integer(testInt);

  ASSERT_EQ(integer.toInteger(), testInt);

  // Assignment
  Variant assignment(false);
  assignment = integer;
  ASSERT_EQ(assignment.toInteger(), testInt);

  // Copy
  Variant copy(integer);
  ASSERT_EQ(copy.toInteger(), testInt);
}

TEST_F(VariantTest, double)
{
  double testDouble = 47.7;
  Variant d(testDouble);

  ASSERT_EQ(d.toDouble(), testDouble);

  // Assignment
  Variant assignment(-1);
  assignment = d;
  ASSERT_EQ(assignment.toDouble(), testDouble);

  // Copy
  Variant copy(d);
  ASSERT_EQ(copy.toDouble(), testDouble);
}

TEST_F(VariantTest, string)
{
  std::string testString("how long is a piece of string?");
  Variant str(testString);

  ASSERT_EQ(str.toString(), testString);

  // Assignment
  Variant assignment("");
  assignment = str;
  ASSERT_EQ(assignment.toString(), testString);

  // Copy
  Variant copy(str);
  ASSERT_EQ(copy.toString(), testString);
}

TEST_F(VariantTest, list)
{
  std::vector<Variant> testList;
  bool boolValue = true;
  Variant boo(boolValue);
  int intValue = 47;
  Variant integer(intValue);
  double doubleValue = 47.7;
  Variant d(doubleValue);
  std::string strValue = "how long is a piece of string?";
  Variant str(strValue);

  testList.push_back(boo);
  testList.push_back(integer);
  testList.push_back(d);
  testList.push_back(str);

  Variant listVariant(testList);

  auto validate = [=](Variant& list) {
    ASSERT_EQ(list.toList().size(), 4);
    ASSERT_EQ(list.toList()[0].toBool(), boolValue);
    ASSERT_EQ(list.toList()[1].toInteger(), intValue);
    ASSERT_EQ(list.toList()[2].toDouble(), doubleValue);
    ASSERT_EQ(list.toList()[3].toString(), strValue);
  };
  validate(listVariant);

  // Assignment
  Variant assignment("");
  assignment = listVariant;
  validate(assignment);

  // Copy
  Variant copy(listVariant);
  validate(copy);
}

#include <iostream>
#include "gtest/gtest.h"
#include "util.hpp"

using namespace std;

namespace {

TEST(ParseDiceFormula, ShouldSucceeed) {
  DiceFormula dice_formula = ParseDiceFormula("2d10+5");
  EXPECT_EQ(dice_formula.num_die, 2);
  EXPECT_EQ(dice_formula.dice, 10);
  EXPECT_EQ(dice_formula.bonus, 5);

  for (int i = 0; i < 10; i++) {
    int result = ProcessDiceFormula(dice_formula);
    EXPECT_TRUE(result >= 7 && result <= 25);
  }

  dice_formula = ParseDiceFormula("1d5");
  EXPECT_EQ(dice_formula.num_die, 1);
  EXPECT_EQ(dice_formula.dice, 5);
  EXPECT_EQ(dice_formula.bonus, 0);
  for (int i = 0; i < 10; i++) {
    int result = ProcessDiceFormula(dice_formula);
    EXPECT_TRUE(result >= 1 && result <= 5);
  }

  dice_formula = ParseDiceFormula("231");
  EXPECT_EQ(dice_formula.num_die, 0);
  EXPECT_EQ(dice_formula.dice, 0);
  EXPECT_EQ(dice_formula.bonus, 231);

  int result = ProcessDiceFormula(dice_formula);
  EXPECT_TRUE(result == 231);
}

TEST(DiceFormulaToStr, ShouldSucceeed) {
  DiceFormula dice_formula = ParseDiceFormula("2d10+5");
  EXPECT_EQ("2d10+5", DiceFormulaToStr(dice_formula));

  dice_formula = ParseDiceFormula("1d5");
  EXPECT_EQ("1d5", DiceFormulaToStr(dice_formula));

  dice_formula = ParseDiceFormula("12");
  EXPECT_EQ("12", DiceFormulaToStr(dice_formula));
}

TEST(CombinationWithIndex, ShouldSucceeed) {
  vector<int> c = GetCombinationFromIndex(0, 3, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(0, c[0]);
  EXPECT_EQ(1, c[1]);
  EXPECT_EQ(0, GetIndexFromCombination(c, 3, 2));

  c = GetCombinationFromIndex(1, 3, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(0, c[0]);
  EXPECT_EQ(2, c[1]);
  EXPECT_EQ(1, GetIndexFromCombination(c, 3, 2));

  c = GetCombinationFromIndex(2, 3, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(1, c[0]);
  EXPECT_EQ(2, c[1]);
  EXPECT_EQ(2, GetIndexFromCombination(c, 3, 2));

  c = GetCombinationFromIndex(3, 3, 2);
  ASSERT_EQ(0, c.size());

  c = GetCombinationFromIndex(0, 6, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(0, c[0]);
  EXPECT_EQ(1, c[1]);
  EXPECT_EQ(0, GetIndexFromCombination(c, 6, 2));

  c = GetCombinationFromIndex(1, 6, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(0, c[0]);
  EXPECT_EQ(2, c[1]);
  EXPECT_EQ(1, GetIndexFromCombination(c, 6, 2));

  c = GetCombinationFromIndex(5, 6, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(1, c[0]);
  EXPECT_EQ(2, c[1]);
  EXPECT_EQ(5, GetIndexFromCombination(c, 6, 2));

  c = GetCombinationFromIndex(14, 15, 2);
  ASSERT_EQ(2, c.size());
  EXPECT_EQ(1, c[0]);
  EXPECT_EQ(2, c[1]);
  EXPECT_EQ(14, GetIndexFromCombination(c, 15, 2));

  EXPECT_EQ(9, GetIndexFromCombination({ 3, 2 }, 6, 2));
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

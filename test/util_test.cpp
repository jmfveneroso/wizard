#include <iostream>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "util.hpp"

using namespace std;

using ::testing::ElementsAre;

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

TEST(PredictMissileHitLocation, ShouldSucceeed) {
  vec2 hit = PredictMissileHitLocation(vec2(0.0f, 0.0f), 1.0f, 
    vec2(0.0f, 5.0f), vec2(1.0f, 0.0f), 0.2f);

  EXPECT_NEAR(hit.x, 1.02062, 0.01);
  EXPECT_NEAR(hit.y, 5, 0.01);

  hit = PredictMissileHitLocation(vec2(0.0f, 0.0f), 1.0f, 
    vec2(0.0f, 5.0f), vec2(-1.0f, 0.0f), 0.2f);

  EXPECT_NEAR(hit.x, -1.02062, 0.01);
  EXPECT_NEAR(hit.y, 5, 0.01);

  hit = PredictMissileHitLocation(vec2(0.0f, 0.0f), 1.0f, 
    vec2(0.0f, 5.0f), vec2(0.0f, 1.0f), 0.2f);

  EXPECT_NEAR(hit.x, 0, 0.01);
  EXPECT_NEAR(hit.y, 6.44167, 0.01);

  hit = PredictMissileHitLocation(vec2(0.0f, 0.0f), 1.0f, 
    vec2(0.0f, 5.0f), vec2(1.0f, 1.0f), 0.2f);

  EXPECT_NEAR(hit.x, 0.9291, 0.01);
  EXPECT_NEAR(hit.y, 5.9291, 0.01);

  hit = PredictMissileHitLocation(vec2(0.0f, 0.0f), 1.0f, 
    vec2(0.0f, 5.0f), vec2(0.0f, 0.0f), 0.2f);

  EXPECT_NEAR(hit.x, 0.0, 0.01);
  EXPECT_NEAR(hit.y, 5.0, 0.01);
}

TEST(RotateMatrix, ShouldSucceeed) {
  vector<vector<int>> mat {
    {13, 13, 13, 13},
    {1, 1, 2, 1},
    {22, 22, 22, 22},
  };

  vector<vector<int>> result = RotateMatrix(mat);

  ASSERT_THAT(result.size(), 4);

  EXPECT_THAT(result[0], ElementsAre(22, 1, 13));
  EXPECT_THAT(result[1], ElementsAre(22, 1, 13));
  EXPECT_THAT(result[2], ElementsAre(22, 2, 13));
  EXPECT_THAT(result[3], ElementsAre(22, 1, 13));
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

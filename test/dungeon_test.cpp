#include <iostream>
#include "gtest/gtest.h"
#include "dungeon.hpp"

using namespace std;

namespace {

TEST(Dungeon, GenerateDungeon) {
  Dungeon dungeon;
  dungeon.GenerateDungeon();
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

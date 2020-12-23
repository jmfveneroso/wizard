#include <iostream>
#include "gtest/gtest.h"

using namespace std;

namespace {

class ExampleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    x_ = 13;
  }

  int x_;
};

int ExampleFunction() {
  return 1;
}

TEST_F(ExampleTest, ExampleClassTest) {
  EXPECT_EQ(x_, 13);
  ASSERT_FALSE(x_ == 123);
}

TEST(FunctionTest, ExampleFunctionTest) {
  EXPECT_EQ(ExampleFunction(), 1);
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

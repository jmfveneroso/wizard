#include <iostream>
#include "gtest/gtest.h"
#include "fbx_loader.hpp"

using namespace std;

namespace {

TEST(Fbx, TestAnimationExtraction) {
  FbxData data;
  FbxLoad("resources/models_fbx/cube_test.fbx", data);
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

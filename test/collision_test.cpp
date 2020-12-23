#include <iostream>
#include "gtest/gtest.h"
#include "collision.hpp"

using namespace std;

namespace {

void ExpectVec3Eq(const vec3& a, const vec3& b) {
  EXPECT_EQ(a.x, a.x);
  EXPECT_EQ(a.y, a.y);
  EXPECT_EQ(a.z, a.z);
}

TEST(Collisions, TestSphereSphere) {
  BoundingSphere s1(vec3(0, 0, 0), 3);
  BoundingSphere s2(vec3(0, 1, 0), 1);

  vec3 displacement_vector;
  vec3 point_of_contact;
  bool result = TestSphereSphere(s1, s2, displacement_vector, point_of_contact);

  EXPECT_TRUE(result);
  ExpectVec3Eq(vec3(0, -1, 0), displacement_vector);
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

TEST(Collisions, TestIntersectRayAABB) {
  vec3 p = vec3(0, 0, 0);
  vec3 d = vec3(1, 0, 0);

  AABB aabb(vec3(5, -5, -5), vec3(10, 10, 10));

  float tmin;
  vec3 q;
  bool result = IntersectRayAABB(p, d, aabb, tmin, q);

  EXPECT_TRUE(result);

  d = vec3(0, 0, 1);
  result = IntersectRayAABB(p, d, aabb, tmin, q);
  EXPECT_FALSE(result);

  aabb = AABB(vec3(-1, -1, -1), vec3(5, 5, 5));
  p = vec3(0.0, 100.0, 0.0);
  d = vec3(0.1, 1.0, 0.1);
  result = IntersectRayAABB(p, d, aabb, tmin, q);
  EXPECT_FALSE(result);

  aabb = AABB(vec3(-1, -1, -1), vec3(5, 5, 5));
  p = vec3(0.0, 100.0, 0.0);
  d = vec3(0.0, -1.0, 0.0);
  result = IntersectRayAABB(p, d, aabb, tmin, q);
  EXPECT_TRUE(result);
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

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

TEST(Collisions, TestIntersectSphereAABB) {
  AABB aabb = AABB(vec3(11600, -100, 7600), vec3(400, 100, 400)); 
  BoundingSphere s = BoundingSphere(vec3(11710, -3, 7710), 1.5);

  vec3 v;
  vec3 p;
  bool collided = IntersectSphereAABB(s, aabb, v, p);

  EXPECT_TRUE(collided);
  EXPECT_NEAR(v[0], 0, 0.01);
  EXPECT_NEAR(v[1], 4.5, 0.01);
  EXPECT_NEAR(v[2], 0, 0.01);
}

} // End of namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

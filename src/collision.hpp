#ifndef __COLLISION_HPP__
#define __COLLISION_HPP__

#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <exception>
#include <tuple>
#include <memory>
#include <thread>
#include <vector>
#include "util.hpp"

using namespace std;
using namespace glm;

class Resources;

void ExtractFrustumPlanes(const mat4& MVP, vec4 planes[6]);

bool CollideSphereFrustum(const BoundingSphere& bounding_sphere, 
  const vec4 planes[], const vec3& player_pos);

bool CollideAABBFrustum(const AABB& aabb, const vec4 planes[], 
  const vec3& player_pos);

bool CollideTriangleFrustum(const vector<vec3>& v, const vec4 planes[],
  const vec3& player_pos);

vec3 ClosestPtPointTriangle(vec3 p, vec3 a, vec3 b, vec3 c, bool* inside);

vec3 ClosestPtPointSegment(vec3 c, vec3 a, vec3 b);

vec3 ClosestPtPointAABB(const vec3& p, const AABB& aabb);

bool IsBehindPlane(const vec3& p, const vec3& plane_point, const vec3& normal);

bool IsInConvexHull(const vec3& p, vector<Polygon> polygons);

bool IsInConvexHull(const BoundingSphere& bounding_sphere, vector<Polygon> polygons);

bool IsInConvexHull(const AABB& aabb, vector<Polygon> polygons);

bool IsPointInAABB(const vec3& p, const AABB& aabb);

bool TestSphereAABB(const BoundingSphere& s, const AABB& aabb);

bool IntersectSphereAABB(const BoundingSphere& s, const AABB& aabb, 
  vec3& displacement_vector, vec3& point_of_contact);

bool IsAABBIntersectingAABB(const AABB& aabb1, const AABB& aabb2);

bool TestSphereAABBIntersection(const BoundingSphere& s, const AABB& aabb);

bool TestSphereTriangleIntersection(const BoundingSphere& s, const vector<vec3>& v);

Polygon CreatePolygonFrom3Points(vec3 a, vec3 b, vec3 c, vec3 direction);

BoundingSphere GetBoundingSphereFromVertices(const vector<vec3>& vertices);

bool IntersectBoundingSphereWithTriangle(const BoundingSphere& bounding_sphere, 
  const Polygon& polygon, vec3& displacement_vector, vec3& point_of_contact);

OBB GetOBBFromPolygons(const vector<Polygon>& polygons, const vec3& position);

vector<Polygon> GetPolygonsFromOBB(const OBB& obb);

bool IntersectRaySphere(vec3 p, vec3 d, BoundingSphere s, float &t, vec3 &q);

bool TestMovingSphereSphere(BoundingSphere s0, BoundingSphere s1, vec3 v0, 
  vec3 v1, float& t, vec3& q);

bool IntersectMovingSphereTriangle(BoundingSphere s, vec3 v, 
  const Polygon& polygon, float &t, vec3 &q);

BoundingSphere GetBoundingSphereFromPolygons(const vector<Polygon>& polygons);

BoundingSphere GetBoundingSphereFromPolygons(const Polygon& polygon);

BoundingSphere SphereEnclosingSpheres(const BoundingSphere& s0, 
  const BoundingSphere& s1);

shared_ptr<SphereTreeNode> ConstructSphereTreeFromPolygons(
  const vector<Polygon>& polygons);

shared_ptr<AABBTreeNode> ConstructAABBTreeFromPolygons(
  const vector<Polygon>& polygons);

bool TestSphereSphere(const BoundingSphere& s1, const BoundingSphere& s2, 
  vec3& displacement_vector, vec3& point_of_contact);

bool IntersectLineQuad(vec3 p, vec3 q, vec3 a, vec3 b, vec3 c, vec3 d,
  vec3 &r);

bool IntersectRayAABB(vec3 p, vec3 d, AABB a, float &tmin, vec3 &q);

bool IntersectSegmentPlane(vec3 a, vec3 b, Plane p, float &t, vec3 &q);

bool TestOBBPlane(const OBB obb, Plane p, vec3& displacement_vector, 
  vec3& point_of_contact);

bool TestAABBPlane(const AABB& aabb, const Plane& p,
  vec3& displacement_vector, vec3& point_of_contact);

bool TestTriangleAABB(const Polygon& polygon, const AABB& aabb,
  vec3& displacement_vector, vec3& point_of_contact);

bool TestMovingSphereAABB(BoundingSphere s, const AABB& aabb, vec3 d, float& t);

bool IntersectRaySphere(const Ray& ray, BoundingSphere s, float &t, vec3 &q);

bool IntersectSegmentCapsule(const Segment& seg, const Capsule& capsule, 
  float& t);

bool IntersectRayAABBTree(vec3 p, vec3 d, shared_ptr<AABBTreeNode> node, 
  float& t, vec3& q, const vec3& base_position);

bool IntersectObbObb(OBB& a, OBB& b);

vec3 ClosestPtPointPlane(const vec3& q, const Plane& p);

float DistPointPlane(const vec3& q, const Plane& p);

bool IntersectSpherePlane(BoundingSphere s, Plane p, vec3& displacement_vector, 
  vec3& point_of_contact);

// bool IntersectMovingSphereAABB(Sphere s, Vector d, AABB b, float &t);
// https://github.com/vancegroup-mirrors/hapi/blob/master/src/CollisionObjects.cpp


// ObjPtr IntersectRayObjects(const vec3& position, 
//   const vec3& direction, float max_distance=50.0f, 
//   IntersectMode mode = INTERSECT_ITEMS);

#endif // __COLLISION_HPP__.

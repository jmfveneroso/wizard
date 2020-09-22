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

ostream& operator<<(ostream& os, const AABB& v);

void ExtractFrustumPlanes(const mat4& MVP, vec4 planes[6]);

bool CollideAABBFrustum(const AABB& aabb, const vec4 planes[], 
  const vec3& player_pos);

bool CollideTriangleFrustum(const vector<vec3>& v, const vec4 planes[],
  const vec3& player_pos);

vec3 ClosestPtPointTriangle(vec3 p, vec3 a, vec3 b, vec3 c, bool* inside);

vec3 ClosestPtPointSegment(vec3 c, vec3 a, vec3 b);

vec3 ClosestPtPointAABB(const vec3& p, const AABB& aabb);

bool IsBehindPlane(const vec3& p, const vec3& plane_point, const vec3& normal);

bool IsInConvexHull(const vec3& p, vector<Polygon> polygons);

bool IsInConvexHull(const AABB& aabb, vector<Polygon> polygons);

bool IsPointInAABB(const vec3& p, const AABB& aabb);

bool IsAABBIntersectingAABB(const AABB& aabb1, const AABB& aabb2);

bool TestSphereAABBIntersection(const BoundingSphere& s, const AABB& aabb);

bool TestSphereTriangleIntersection(const BoundingSphere& s, const vector<vec3>& v);

Polygon CreatePolygonFrom3Points(vec3 a, vec3 b, vec3 c, vec3 direction);

AABB GetAABBFromVertices(const vector<vec3>& vertices);

BoundingSphere GetBoundingSphereFromVertices(const vector<vec3>& vertices);

bool IntersectWithTriangle(const Polygon& polygon, vec3* player_pos, 
  vec3 old_player_pos, float* magnitude, const vec3& object_pos, float radius = 1.5f);

bool IntersectBoundingSphereWithTriangle(const BoundingSphere& bounding_sphere, 
  const Polygon& polygon, vec3* displacement_vector);

OBB GetOBBFromPolygons(const vector<Polygon>& polygons, const vec3& position);

vector<Polygon> GetPolygonsFromOBB(const OBB& obb);

#endif // __COLLISION_HPP__.

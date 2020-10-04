#ifndef __COLLISION_RESOLVER_HPP__
#define __COLLISION_RESOLVER_HPP__

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
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <png.h>
#include "asset.hpp"
#include "boost/filesystem.hpp"
#include <boost/lexical_cast.hpp>  
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace glm;

struct Collision {
  shared_ptr<GameObject> obj1;
  shared_ptr<GameObject> obj2;
  vec3 displacement_vector;
  vec3 normal;
  Collision() {}
  Collision(shared_ptr<GameObject> obj1, shared_ptr<GameObject> obj2,
    vec3 displacement_vector, vec3 normal) : obj1(obj1), obj2(obj2), 
    displacement_vector(displacement_vector), normal(normal) {}
};

class CollisionResolver {
  double start_time_ = 0;
  int num_collisions_ = 0;
  int num_objects_tested_ = 0;
  vector<Collision> collisions_ = vector<Collision>(1000);
  shared_ptr<AssetCatalog> asset_catalog_;

  void GetPotentiallyCollidingObjects(const vec3& pos, 
    shared_ptr<GameObject> colliding_obj,
    vector<shared_ptr<GameObject>>& objs);

  float GetTerrainHeight(vec2 pos, vec3* normal);

  // Sphere collision.
  bool CollideSphereSphere(const BoundingSphere& sphere1,
    const BoundingSphere& sphere2, vec3* displacement_vector);
  bool CollideSphereSphere(shared_ptr<GameObject> obj1, 
    shared_ptr<GameObject> obj2, vec3* displacement_vector);
  bool CollideSpherePerfect(const BoundingSphere& sphere, 
    const vector<Polygon>& polygons, vec3 position, vec3* displacement_vector);
  bool CollideSpherePerfect(shared_ptr<GameObject> obj1, 
    shared_ptr<GameObject> obj2, vec3* displacement_vector);
  bool CollideSphereBones(const BoundingSphere& sphere, 
    const vector<BoundingSphere>& bone_bounding_spheres, 
    vec3* displacement_vector);
  bool CollideSphereBones(shared_ptr<GameObject> obj1, 
    shared_ptr<GameObject> obj2, vec3* displacement_vector);

  // Quick sphere collision.
  bool CollideQuickSpherePerfect(shared_ptr<GameObject> obj1, 
    shared_ptr<GameObject> obj2, vec3* displacement_vector);
  bool CollideQuickSphereBones(shared_ptr<GameObject> obj1, 
    shared_ptr<GameObject> obj2, vec3* displacement_vector);

  // Bones collision.
  bool CollideBonesPerfect(shared_ptr<GameObject> obj1, 
    shared_ptr<GameObject> obj2, vec3* displacement_vector);

  // TODO: this should be improved for terrain with high steepness.
  void CollideWithTerrain(shared_ptr<GameObject> obj);
  void CollideWithTerrain(shared_ptr<OctreeNode> octree_node);

  void ResolveCollisions();
  bool CollideObjects(shared_ptr<GameObject> obj1,
    shared_ptr<GameObject> obj2, vec3* displacement_vector);
  void FindCollisions(shared_ptr<OctreeNode> octree_node, 
    vector<shared_ptr<GameObject>> objs);

 public:
  CollisionResolver(shared_ptr<AssetCatalog> asset_catalog);

  void Collide();
};

#endif // __UTIL_HPP__

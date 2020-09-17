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

struct CollisionResolution {
  vec3 displacement_vector;
};

class CollisionResolver {
  shared_ptr<AssetCatalog> asset_catalog_;

  void GetPotentiallyCollidingObjects(const vec3& player_pos, 
    shared_ptr<OctreeNode> octree_node,
    vector<shared_ptr<GameObject>>& objects);

  void CollidePlayerWithSector(shared_ptr<StabbingTreeNode> stabbing_tree_node, 
    vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump);

  // Magic Missile, Spider code.
  MagicMissile magic_missiles_[10];

  float GetTerrainHeight(vec2 pos, vec3* normal);
  void CollideWithTerrain(shared_ptr<GameObject> obj);
  bool CollideWithObject(const BoundingSphere& bounding_sphere,
    shared_ptr<GameObject> obj, vec3* displacement_vector);
  void CollidePlayerWithObjects(vec3* player_pos, vec3* player_speed, 
    bool* can_jump, const vector<shared_ptr<GameObject>>& objs);
  void CollideSpiderWithObjects(
    shared_ptr<GameObject> spider,
    const vector<shared_ptr<GameObject>>& objs);

 public:
  CollisionResolver(shared_ptr<AssetCatalog> asset_catalog);

  void Collide(vec3* player_pos, vec3 old_player_pos, vec3* player_speed, 
    bool* can_jump);

  void CollideSpider();

  void InitMagicMissile();
  void ChargeMagicMissile(const Camera& camera);
  void CastMagicMissile(const Camera& camera);
  void UpdateMagicMissile(const Camera& camera);
};

#endif // __UTIL_HPP__

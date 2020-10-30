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

// Not all collision pairs are possible.
enum CollisionPair {
  CP_SS = 0, // SPHERE -> SPHERE
  CP_SB,     // SPHERE -> BONES
  CP_SQ,     // SPHERE -> QUICK_SPHERE
  CP_SP,     // SPHERE -> PERFECT
  CP_ST,     // SPHERE -> TERRAIN (Not implemented)             
  CP_BS,     // BONES -> SPHERE
  CP_BB,     // BONES -> BONES  (Not implemented)
  CP_BQ,     // BONES -> QUICK_SPHERE 
  CP_BP,     // BONES -> PERFECT
  CP_BT,     // BONES -> TERRAIN (Not implemented)             
  CP_QS,     // QUICK_SPHERE -> SPHERE (Not implemented)
  CP_QB,     // QUICK_SPHERE -> BONES
  CP_QQ,     // QUICK_SPHERE -> QUICK_SPHERE (Not implemented)
  CP_QP,     // QUICK_SPHERE -> PERFECT
  CP_QT,     // QUICK_SPHERE -> TERRAIN (Not implemented)             
  CP_PS,     // PERFECT -> SPHERE
  CP_PB,     // PERFECT -> BONES
  CP_PQ,     // PERFECT -> QUICK_SPHERE
  CP_PP,     // PERFECT -> PERFECT (Not implemented)             
  CP_PT,     // PERFECT -> TERRAIN (Not implemented)             
  CP_TS,     // TERRAIN -> SPHERE
  CP_TB,     // TERRAIN -> BONES
  CP_TQ,     // TERRAIN -> QUICK_SPHERE
  CP_TP,     // TERRAIN -> PERFECT (Not implemented)             
  CP_TT,     // TERRAIN -> TERRAIN (Not implemented)             
  CP_UNDEFINED 
};

struct Collision {
  CollisionPair collision_pair = CP_UNDEFINED;
  bool collided = false;

  shared_ptr<GameObject> obj1, obj2;

  // The point where the collision happened.
  vec3 point_of_contact;
 
  // Collision face normal.
  vec3 normal;

  // How should obj1 move to resolve the collision.
  vec3 displacement_vector;

  // When (time t) in the movement of obj1 and obj2 the collision happened.
  float obj1_t;
  float obj2_t;

  Collision() {}
  Collision(CollisionPair collision_pair, ObjPtr obj1, ObjPtr obj2) : 
    collision_pair(collision_pair), obj1(obj1), obj2(obj2) {}
  Collision(ObjPtr obj1, ObjPtr obj2, vec3 displacement_vector, vec3 normal) 
    : obj1(obj1), obj2(obj2), displacement_vector(displacement_vector), 
      normal(normal) {}
};
using ColPtr = shared_ptr<Collision>;

struct CollisionSS : Collision {
  CollisionSS() {}
  CollisionSS(ObjPtr o1, ObjPtr o2) : Collision(CP_SS, o1, o2) {}
};

struct CollisionSB : Collision {
  ObjPtr bone;
  CollisionSB() {}
  CollisionSB(ObjPtr o1, ObjPtr o2, ObjPtr bone)
    : Collision(CP_SB, o1, o2), bone(bone) {}
};

struct CollisionSP : Collision {
  Polygon polygon;
  CollisionSP() {}
  CollisionSP(ObjPtr o1, ObjPtr o2, Polygon polygon)
    : Collision(CP_SP, o1, o2), polygon(polygon) {}
};

struct CollisionST : Collision {
  CollisionST() {}
  CollisionST(ObjPtr o1) : Collision(CP_ST, o1, nullptr) {}
};

struct CollisionBB : Collision {
  ObjPtr bone1;
  ObjPtr bone2;
  CollisionBB() {}
  CollisionBB(ObjPtr o1, ObjPtr o2, ObjPtr b1, ObjPtr b2) 
    : Collision(CP_BB, o1, o2), bone1(b1), bone2(b2) {}
};

struct CollisionBP : Collision {
  ObjPtr bone;
  Polygon polygon;
  CollisionBP() {}
  CollisionBP(ObjPtr o1, ObjPtr o2, ObjPtr bone, Polygon polygon)
    : Collision(CP_BP, o1, o2), bone(bone), polygon(polygon) {}
};

struct CollisionBT : Collision {
  ObjPtr bone;
  CollisionBT() {}
  CollisionBT(ObjPtr o1, ObjPtr bone) 
    : Collision(CP_BT, o1, nullptr), bone(bone) {}
};

struct CollisionQS : Collision {
  CollisionQS() {}
  CollisionQS(ObjPtr o1, ObjPtr o2) : Collision(CP_QS, o1, o2) {}
};

struct CollisionQP : Collision {
  Polygon polygon;
  CollisionQP() {}
  CollisionQP(ObjPtr o1, ObjPtr o2, Polygon polygon)
    : Collision(CP_QP, o1, o2), polygon(polygon) {}
};

struct CollisionQB : Collision {
  ObjPtr bone;
  CollisionQB() {}
  CollisionQB(ObjPtr o1, ObjPtr o2, ObjPtr bone)
    : Collision(CP_QB, o1, o2), bone(bone) {}
};

struct CollisionQT : Collision {
  CollisionQT() {}
  CollisionQT(ObjPtr o1) : Collision(CP_QT, o1, nullptr) {}
};

class CollisionResolver {
  shared_ptr<AssetCatalog> asset_catalog_;

  vector<ColPtr> collisions_;

  // Statistics.
  double start_time_ = 0;
  int num_objects_tested_ = 0;
  int perfect_collision_tests_ = 0;
  void ClearMetrics();
  void PrintMetrics();

  // Aux methods.
  void GetTerrainPolygons(vec2 pos, vector<Polygon>& polygons);
  float GetTerrainHeight(vec2 pos, vec3* normal);
  bool IsPairCollidable(ObjPtr obj1, ObjPtr obj2);
  void CollideAlongAxis(shared_ptr<OctreeNode> octree_node, ObjPtr obj);

  void TestCollisionSS(shared_ptr<CollisionSS> c);
  void TestCollisionSB(shared_ptr<CollisionSB> c);
  void TestCollisionSP(shared_ptr<CollisionSP> c);
  void TestCollisionST(shared_ptr<CollisionST> c);
  void TestCollisionBB(shared_ptr<CollisionBB> c);
  void TestCollisionBP(shared_ptr<CollisionBP> c);
  void TestCollisionBT(shared_ptr<CollisionBT> c);
  void TestCollisionQS(shared_ptr<CollisionQS> c);
  void TestCollisionQP(shared_ptr<CollisionQP> c);
  void TestCollisionQB(shared_ptr<CollisionQB> c);
  void TestCollisionQT(shared_ptr<CollisionQT> c);
  void TestCollision(ColPtr c);

  vector<ColPtr> CollideObjects(ObjPtr obj1, ObjPtr obj2);

  void UpdateObjectPositions();
  void FindCollisions(shared_ptr<OctreeNode> octree_node, vector<ObjPtr> objs);
  // TODO: this should be improved for terrain with high steepness.
  void FindCollisionsWithTerrain();
  void ResolveCollisions();

 public:
  CollisionResolver(shared_ptr<AssetCatalog> asset_catalog);

  void Collide();
};

#endif // __UTIL_HPP__

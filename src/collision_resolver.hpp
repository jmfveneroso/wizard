#ifndef __COLLISION_RESOLVER_HPP__
#define __COLLISION_RESOLVER_HPP__

#include "resources.hpp"

#include <thread>
#include <mutex>

// Not all collision pairs are possible.
enum CollisionPair {
  CP_SS = 0, // SPHERE -> SPHERE
  CP_SB,     // SPHERE -> BONES
  CP_SQ,     // SPHERE -> QUICK_SPHERE
  CP_SP,     // SPHERE -> PERFECT
  CP_ST,     // SPHERE -> TERRAIN
  CP_SH,     // SPHERE -> C. HULL (Not implemented)
  CP_SO,     // SPHERE -> OBB
  CP_BS,     // BONES -> SPHERE
  CP_BB,     // BONES -> BONES
  CP_BQ,     // BONES -> QUICK_SPHERE 
  CP_BP,     // BONES -> PERFECT
  CP_BT,     // BONES -> TERRAIN
  CP_BH,     // BONES -> C. HULL (Not implemented)
  CP_BO,     // BONES -> OBB (Not implemented)
  CP_QS,     // QUICK_SPHERE -> SPHERE
  CP_QB,     // QUICK_SPHERE -> BONES
  CP_QQ,     // QUICK_SPHERE -> QUICK_SPHERE (Not implemented)
  CP_QP,     // QUICK_SPHERE -> PERFECT
  CP_QT,     // QUICK_SPHERE -> TERRAIN
  CP_QH,     // QUICK_SPHERE -> C. HULL
  CP_QO,     // QUICK_SPHERE -> OBB (Not implemented)
  CP_PS,     // PERFECT -> SPHERE
  CP_PB,     // PERFECT -> BONES
  CP_PQ,     // PERFECT -> QUICK_SPHERE
  CP_PP,     // PERFECT -> PERFECT (Not implemented)             
  CP_PT,     // PERFECT -> TERRAIN (Not implemented)             
  CP_PH,     // PERFECT -> C. HULL (Not implemented)
  CP_PO,     // PERFECT -> OBB (Not implemented)
  CP_TS,     // TERRAIN -> SPHERE
  CP_TB,     // TERRAIN -> BONES
  CP_TQ,     // TERRAIN -> QUICK_SPHERE
  CP_TP,     // TERRAIN -> PERFECT (Not implemented)             
  CP_TT,     // TERRAIN -> TERRAIN (Not implemented)             
  CP_TH,     // TERRAIN -> C. HULL (Not implemented)             
  CP_TO,     // TERRAIN -> OBB (Not implemented)             
  CP_HS,     // C. HULL -> SPHERE (Not implemented)
  CP_HB,     // C. HULL -> BONES (Not implemented)
  CP_HQ,     // C. HULL -> QUICK_SPHERE
  CP_HP,     // C. HULL -> PERFECT (Not implemented)             
  CP_HT,     // C. HULL -> TERRAIN
  CP_HH,     // C. HULL -> C. HULL (Not implemented)             
  CP_HO,     // C. HULL -> OBB (Not implemented)             
  CP_OS,     // OBB -> SPHERE (Not implemented)
  CP_OB,     // OBB -> BONES (Not implemented)
  CP_OQ,     // OBB -> QUICK_SPHERE (Not implemented)
  CP_OP,     // OBB -> PERFECT
  CP_OT,     // OBB -> TERRAIN
  CP_OH,     // OBB -> C. HULL (Not implemented)             
  CP_OO,     // OBB -> OBB (Not implemented)             
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
  int bone;
  CollisionSB() {}
  CollisionSB(ObjPtr o1, ObjPtr o2, int bone)
    : Collision(CP_SB, o1, o2), bone(bone) {}
};

struct CollisionSP : Collision {
  Polygon polygon;
  CollisionSP() {}
  CollisionSP(ObjPtr o1, ObjPtr o2, Polygon polygon)
    : Collision(CP_SP, o1, o2), polygon(polygon) {}
};

struct CollisionSH : Collision {
  Polygon polygon;
  CollisionSH() {}
  CollisionSH(ObjPtr o1, ObjPtr o2, Polygon polygon)
    : Collision(CP_SH, o1, o2), polygon(polygon) {}
};

struct CollisionSO : Collision {
  CollisionSO() {}
  CollisionSO(ObjPtr o1, ObjPtr o2)
    : Collision(CP_SO, o1, o2) {}
};

struct CollisionST : Collision {
  Polygon polygon;
  CollisionST() {}
  CollisionST(ObjPtr o1, Polygon polygon) 
    : Collision(CP_ST, o1, nullptr), polygon(polygon) {}
};

struct CollisionBB : Collision {
  int bone1;
  int bone2;
  CollisionBB() {}
  CollisionBB(ObjPtr o1, ObjPtr o2, int b1, int b2) 
    : Collision(CP_BB, o1, o2), bone1(b1), bone2(b2) {}
};

struct CollisionBP : Collision {
  int bone;
  Polygon polygon;
  CollisionBP() {}
  CollisionBP(ObjPtr o1, ObjPtr o2, int bone, Polygon polygon)
    : Collision(CP_BP, o1, o2), bone(bone), polygon(polygon) {}
};

struct CollisionBO : Collision {
  int bone;
  CollisionBO() {}
  CollisionBO(ObjPtr o1, ObjPtr o2, int bone)
    : Collision(CP_BO, o1, o2), bone(bone) {}
};

struct CollisionBT : Collision {
  int bone;
  Polygon polygon;
  CollisionBT() {}
  CollisionBT(ObjPtr o1, int bone, Polygon polygon) 
    : Collision(CP_BT, o1, nullptr), bone(bone), polygon(polygon) {}
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
  int bone;
  CollisionQB() {}
  CollisionQB(ObjPtr o1, ObjPtr o2, int bone)
    : Collision(CP_QB, o1, o2), bone(bone) {}
};

struct CollisionQT : Collision {
  CollisionQT() {}
  CollisionQT(ObjPtr o1) : Collision(CP_QT, o1, nullptr) {}
};

struct CollisionQH : Collision {
  Polygon polygon;
  CollisionQH() {}
  CollisionQH(ObjPtr o1, ObjPtr o2, Polygon p) 
    : Collision(CP_QH, o1, o2), polygon(p) {}
};

struct CollisionHT : Collision {
  Polygon polygon;
  CollisionHT() {}
  CollisionHT(ObjPtr o1, Polygon p) 
    : Collision(CP_HT, o1, nullptr), polygon(p) {}
};

struct CollisionOP : Collision {
  Polygon polygon;
  CollisionOP() {}
  CollisionOP(ObjPtr o1, ObjPtr o2, Polygon polygon)
    : Collision(CP_OP, o1, o2), polygon(polygon) {}
};

struct CollisionOT : Collision {
  Polygon polygon;
  CollisionOT() {}
  CollisionOT(ObjPtr o1, Polygon polygon)
    : Collision(CP_OT, o1, nullptr), polygon(polygon) {}
};

class CollisionResolver {
  shared_ptr<Resources> resources_;

  queue<ColPtr> collisions_;

  // Statistics.
  double start_time_ = 0;
  int num_objects_tested_ = 0;
  int perfect_collision_tests_ = 0;

  // Parallelism.
  bool terminate_ = false;
  const int kMaxThreads = 16;
  vector<thread> find_threads_;
  vector<thread> test_threads_;

  int running_find_tasks_ = 0;
  int running_test_tasks_ = 0;
  queue<shared_ptr<OctreeNode>> find_tasks_;

  mutex test_mutex_;
  mutex find_mutex_;
  queue<tuple<ObjPtr, ObjPtr>> tentative_pairs_;

  void ClearMetrics();
  void PrintMetrics();

  // Aux methods.
  bool IsPairCollidable(ObjPtr obj1, ObjPtr obj2);
  void CollideAlongAxis(shared_ptr<OctreeNode> octree_node, ObjPtr obj);

  void TestCollisionSS(shared_ptr<CollisionSS> c);
  void TestCollisionSB(shared_ptr<CollisionSB> c);
  void TestCollisionSP(shared_ptr<CollisionSP> c);
  void TestCollisionSH(shared_ptr<CollisionSH> c);
  void TestCollisionSO(shared_ptr<CollisionSO> c);
  void TestCollisionST(shared_ptr<CollisionST> c);
  void TestCollisionBB(shared_ptr<CollisionBB> c);
  void TestCollisionBP(shared_ptr<CollisionBP> c);
  void TestCollisionBO(shared_ptr<CollisionBO> c);
  void TestCollisionBT(shared_ptr<CollisionBT> c);
  void TestCollisionQS(shared_ptr<CollisionQS> c);
  void TestCollisionQP(shared_ptr<CollisionQP> c);
  void TestCollisionQB(shared_ptr<CollisionQB> c);
  void TestCollisionQT(shared_ptr<CollisionQT> c);
  void TestCollisionQH(shared_ptr<CollisionQH> c);
  void TestCollisionHT(shared_ptr<CollisionHT> c);
  void TestCollisionOP(shared_ptr<CollisionOP> c);
  void TestCollisionOT(shared_ptr<CollisionOT> c);
  void TestCollision(ColPtr c);

  vector<ColPtr> CollideObjects(ObjPtr obj1, ObjPtr obj2);

  void UpdateObjectPositions();
  void FindCollisions(shared_ptr<OctreeNode> octree_node);

  void TestCollisionsWithTerrain();
  void ResolveMissileCollision(ColPtr c);
  void ResolveCollisions();
  void ProcessInContactWith();

  void ProcessTentativePair(ObjPtr obj1, ObjPtr obj2);

  void FindCollisionsAsync();
  void ProcessTentativePairAsync();
  void CreateThreads();

 public:
  CollisionResolver(shared_ptr<Resources> asset_catalog);
  ~CollisionResolver();

  void Collide();
};

#endif // __UTIL_HPP__

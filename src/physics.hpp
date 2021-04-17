#ifndef __PHYSICS_HPP__
#define __PHYSICS_HPP__

#include "resources.hpp"

class Physics {
 shared_ptr<Resources> resources_;

  void RunPhysicsForMissiles(shared_ptr<OctreeNode> node);
  void RunPhysicsForObject(ObjPtr obj);
  void RunPhysicsInOctreeNode(shared_ptr<OctreeNode> node);

 public:
  Physics(shared_ptr<Resources> asset_catalog);

  void Run();
};

#endif // __PHYSICS_HPP__

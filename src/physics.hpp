#ifndef __PHYSICS_HPP__
#define __PHYSICS_HPP__

#include "resources.hpp"

class Physics {
 shared_ptr<Resources> resources_;

 public:
  Physics(shared_ptr<Resources> asset_catalog);

  void Run();
};

#endif // __PHYSICS_HPP__

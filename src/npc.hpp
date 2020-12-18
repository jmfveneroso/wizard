#ifndef __NPC_HPP__
#define __NPC_HPP__

#include "resources.hpp"
#include "ai.hpp"

class Npc {
  shared_ptr<Resources> resources_;
  shared_ptr<AI> ai_;

  void ProcessNpc(ObjPtr npc);

 public:
  Npc(shared_ptr<Resources> asset_catalog, shared_ptr<AI> ai);

  void ProcessNpcs();
};

#endif

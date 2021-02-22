#ifndef __NPC_HPP__
#define __NPC_HPP__

#include "resources.hpp"
#include "ai.hpp"

class Npc {
  shared_ptr<Resources> resources_;
  shared_ptr<AI> ai_;

  void ProcessFisherman(ObjPtr npc);
  void ProcessBirdTamer(ObjPtr npc);
  void ProcessHuntress(ObjPtr npc);
  void ProcessInnkeep(ObjPtr npc);
  void ProcessFarmer(ObjPtr npc);
  void ProcessBlacksmith(ObjPtr npc);
  void ProcessLeader(ObjPtr npc);
  void ProcessAlchemist(ObjPtr npc);
  void ProcessLibrarian(ObjPtr npc);

 public:
  Npc(shared_ptr<Resources> asset_catalog, shared_ptr<AI> ai);

  void ProcessNpcs();
};

#endif

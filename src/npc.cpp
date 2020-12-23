#include "npc.hpp"

Npc::Npc(shared_ptr<Resources> asset_catalog, shared_ptr<AI> ai) 
  : resources_(asset_catalog), ai_(ai) {
}

void Npc::ProcessNpc(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float angle = atan2(configs->sun_position.x, configs->sun_position.y);

  shared_ptr<Waypoint> wp;
  if (angle > 0) {
    wp = resources_->GetWaypointByName("waypoint-021");
  } else {
    wp = resources_->GetWaypointByName("waypoint-020");
  }

  if (npc->actions.size() <= 1) {
    float dist_next_location = length(npc->position - wp->position);
    if (dist_next_location > 100.0f) {
      if (!npc->actions.empty()) {
        npc->actions.pop();
        npc->frame = 0;
      }
      npc->actions.push(make_shared<MoveAction>(wp->position));
      npc->actions.push(make_shared<StandAction>());
    }
  }
}

void Npc::ProcessNpcs() {
  for (ObjPtr npc : resources_->GetMovingObjects()) {
    if (npc->GetAsset()->name != "fisherman-body") continue;
    ProcessNpc(npc);
  }
}

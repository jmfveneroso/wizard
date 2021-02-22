#include "npc.hpp"

Npc::Npc(shared_ptr<Resources> asset_catalog, shared_ptr<AI> ai) 
  : resources_(asset_catalog), ai_(ai) {
}

void Npc::ProcessFisherman(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-fisherman-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-fisherman-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessBirdTamer(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-bird-tamer-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-bird-tamer-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessHuntress(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-huntress-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-huntress-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessInnkeep(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-inkeep-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-inkeep-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessFarmer(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-farmer-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-farmer-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessBlacksmith(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-blacksmith-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-blacksmith-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessLibrarian(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-librarian-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-librarian-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessLeader(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-leader-001");
  if ((time > 12.0f && time < 18.0f) || (time > 0.0f && time < 6.0f)) {
    wp = resources_->GetWaypointByName("waypoint-leader-002");
  } else if (time > 18.0f && time < 24.0f) {
    wp = resources_->GetWaypointByName("waypoint-leader-003");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessAlchemist(ObjPtr npc) {
  shared_ptr<Configs> configs = resources_->GetConfigs();
  float time = configs->time_of_day;

  shared_ptr<Waypoint> wp = resources_->GetWaypointByName("waypoint-alchemist-001");
  if (int(time) % 2 == 0) {
    wp = resources_->GetWaypointByName("waypoint-alchemist-002");
  }

  float dist_next_location = length(npc->position - wp->position);
  if (dist_next_location > 10.0f) {
    npc->actions = {};
    npc->actions.push(make_shared<MoveAction>(wp->position));
    npc->actions.push(make_shared<StandAction>());
  }
}

void Npc::ProcessNpcs() {
  for (ObjPtr npc : resources_->GetMovingObjects()) {
    if (npc->name == "fisherman-001") {
      ProcessFisherman(npc);
    } else if (npc->name == "leader") {
      ProcessLeader(npc);
    } else if (npc->name == "alchemist") {
      ProcessAlchemist(npc);
    } else if (npc->name == "blacksmith-man") {
      ProcessBlacksmith(npc);
    } else if (npc->name == "bird-tamer") {
      ProcessBirdTamer(npc);
    } else if (npc->name == "huntress") {
      ProcessHuntress(npc);
    } else if (npc->name == "farmer") {
      ProcessFarmer(npc);
    } else if (npc->name == "innkeep") {
      ProcessInnkeep(npc);
    } else if (npc->name == "librarian") {
      ProcessLibrarian(npc);
    }
  }
}


#include "ai.hpp"

AI::AI(shared_ptr<Resources> asset_catalog) : resources_(asset_catalog) {
}

void AI::ChangeState(ObjPtr obj, AiState state) {
  obj->ai_state = state;
  obj->frame = 0;
  obj->state_changed_at = glfwGetTime();
}

ObjPtr AI::GetClosestUnit(ObjPtr spider) {
  float min_distance = 99999.9f;
  ObjPtr closest_unit = nullptr;
  for (ObjPtr obj1 : resources_->GetMovingObjects()) {
    if (obj1->GetAsset()->name != "spider") continue;
    float distance = length(spider->position - obj1->position);
    if (distance < min_distance) {
      closest_unit = obj1;
    } 
  }
  return closest_unit;
}

shared_ptr<Waypoint> AI::GetClosestWaypoint(const vec3& position) {
  const unordered_map<string, shared_ptr<Waypoint>>& waypoints = 
    resources_->GetWaypoints();

  shared_ptr<Waypoint> res_waypoint = nullptr;
  float min_distance = 99999999.9f;
  for (const auto& [name, waypoint] : waypoints) {
    float distance = length(waypoint->position - position);

    if (distance > min_distance) continue;
    min_distance = distance;
    res_waypoint = waypoint;
  }
  return res_waypoint;
}

bool AI::RotateSpider(ObjPtr spider, vec3 point, float rotation_threshold) {
  float turn_rate = spider->GetAsset()->base_turn_rate;

  vec3 to_point = point - spider->position;
  to_point.y = 0;

  const vec3 front = vec3(0, 0, 1);
  const vec3 up = vec3(0, 1, 0);
 
  // At rest position, the object forward direction is +Z and up is +Y.
  spider->forward = normalize(to_point);
  quat h_rotation = RotationBetweenVectors(front, spider->forward);
  quat v_rotation = RotationBetweenVectors(up, spider->up);
  quat target_rotation = v_rotation * h_rotation;

  // Check if object is facing the correct direction.
  vec3 cur_forward = spider->rotation_matrix * front;
  cur_forward.y = 0;
  quat cur_h_rotation = RotationBetweenVectors(front, 
    normalize(cur_forward));

  bool is_rotating = abs(dot(h_rotation, cur_h_rotation)) < rotation_threshold;
  if (dot(spider->cur_rotation, target_rotation) < 0.999f) {
    spider->cur_rotation =  
      RotateTowards(spider->cur_rotation, target_rotation, turn_rate);
  }
  spider->rotation_matrix = mat4_cast(spider->cur_rotation);
  return is_rotating; 
}

void AI::Attack(ObjPtr spider) {
  if (resources_->GetConfigs()->disable_attacks) {
    ChangeState(spider, WANDER);
    return;
  }

  if (!spider->actions.empty()) {
    // Complete the actions before choosing other actions.
    return;
  }

  spider->actions.push(make_shared<TakeAimAction>());
  spider->actions.push(make_shared<RangedAttackAction>());
  spider->actions.push(make_shared<ChangeStateAction>(WANDER));
}

void AI::Chase(ObjPtr spider) {
  if (!spider->actions.empty()) {
    // Complete the actions before choosing other actions.
    return;
  }

  int dice = rand() % 3; 
  ObjPtr player = resources_->GetObjectByName("player");
  vec3 dir = player->position - spider->position;
  if (length(dir) > 100 && dice < 2) {
    vec3 next_pos = spider->position + normalize(dir) * 50.0f;
    spider->actions.push(make_shared<MoveAction>(next_pos));
  } else {
    spider->actions.push(make_shared<TakeAimAction>());
    spider->actions.push(make_shared<RangedAttackAction>());
    spider->actions.push(make_shared<IdleAction>(2));
  }

  if (glfwGetTime() > spider->state_changed_at + 30) {
    spider->actions = {};
    spider->actions.push(make_shared<ChangeStateAction>(WANDER));
  }
}

void AI::Wander(ObjPtr spider) {
  if (!spider->actions.empty()) {
    // Complete the actions before choosing other actions.
    return;
  }

  shared_ptr<Waypoint> closest_wp = GetClosestWaypoint(spider->position);
  if (closest_wp->name == "waypoint-009" || closest_wp->name == "waypoint-014") {
    vec3 destination = closest_wp->next_waypoints[0]->position;
    spider->actions.push(make_shared<MoveAction>(destination));
    return;
  }

  if (glfwGetTime() > spider->state_changed_at + 20) {
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = player->position - spider->position;
    ObjPtr closest_unit = GetClosestUnit(spider);
    if (length(spider->position - closest_unit->position) < 50) {
      spider->actions = {};
      spider->actions.push(make_shared<MoveAction>(closest_wp->position));
      shared_ptr<Waypoint> next_wp = closest_wp->next_waypoints[0];
      spider->actions.push(make_shared<MoveAction>(next_wp->position));
      spider->actions.push(make_shared<IdleAction>(5));
      return;
    }
  }

  ObjPtr player = resources_->GetObjectByName("player");
  vec3 dir = player->position - spider->position;
  if (length(dir) < 100) {
    spider->actions = {};
    spider->actions.push(make_shared<ChangeStateAction>(CHASE));
    return;
  }

  std::normal_distribution<float> distribution(0.0, 25.0);
  float x = distribution(generator_);
  float y = distribution(generator_);

  vec3 destination = closest_wp->position + vec3(x, 0, y);
  spider->actions.push(make_shared<MoveAction>(destination));
  spider->actions.push(make_shared<IdleAction>(5));
}

bool AI::ProcessStatus(ObjPtr spider) {
  // Check status. If taking hit, or dying.
  switch (spider->status) {
    case STATUS_TAKING_HIT: {
      spider->active_animation = "Armature|hit";
      if (spider->frame >= 39) {
        spider->actions = {};
        spider->actions.push(make_shared<ChangeStateAction>(CHASE));
        spider->status = STATUS_NONE;
      }
      return false;
    }
    case STATUS_DYING: {
      spider->active_animation = "Armature|death";
      if (spider->frame >= 78) {
        resources_->CreateParticleEffect(64, spider->position, 
          vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
        spider->status = STATUS_DEAD;
      }
      return false;
    }
    case STATUS_NONE:
    case STATUS_DEAD:
    default:
      break;
  }
  return true;
}

// Given the environment and the queued actions, what should be the next 
// actions? 
void AI::ProcessMentalState(ObjPtr spider) {
  if (spider->GetAsset()->name != "spider") return;

  // TODO: mental states should be Attack, Flee, Patrol (search), Hunt, Wander.
  switch (spider->ai_state) {
    case WANDER: Wander(spider); break;
    case AI_ATTACK: Attack(spider); break;
    case CHASE: Chase(spider); break;
    default: break;
  }
}

bool AI::ProcessRangedAttackAction(ObjPtr spider, 
  shared_ptr<RangedAttackAction> action) {
  spider->active_animation = "Armature|attack";

  if (spider->frame >= 79) {
    return true;
  }

  if (spider->frame == 44) {
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = player->position - spider->position;

    vec3 forward = normalize(vec3(dir.x, 0, dir.z));
    vec3 right = normalize(cross(forward, vec3(0, 1, 0)));

    float x = dot(dir, forward);
    float y = dot(dir, vec3(0, 1, 0));
    float v = 4.0f;
    float v2 = v * v;
    float v4 = v * v * v * v;
    float g = 0.016f;
    float x2 = x * x;

    // https://en.wikipedia.org/wiki/Projectile_motion
    // Angle required to hit coordinate.
    float tan = (v2 - sqrt(v4 - g * (g * x2 + 2 * y * v2))) / (g * x);
    float angle = atan(tan); 
    dir = vec3(rotate(mat4(1.0f), angle, right) * vec4(forward, 1.0f));  

    resources_->CreateParticleEffect(40, 
      spider->position + dir * 5.0f + vec3(0, 2.0, 0), 
      dir * 5.0f, vec3(0.0, 1.0, 0.0), -1.0, 40.0f, 3.0f);

    resources_->SpiderCastMagicMissile(spider, dir);
  }
  return false;
}

bool AI::ProcessChangeStateAction(ObjPtr spider, 
  shared_ptr<ChangeStateAction> action) {
  ChangeState(spider, action->new_state);
  return true;
}

bool AI::ProcessTakeAimAction(ObjPtr spider, shared_ptr<TakeAimAction> action) {
  spider->active_animation = "Armature|walking";

  ObjPtr player = resources_->GetObjectByName("player");
  bool is_rotating = RotateSpider(spider, player->position, 0.99f);
  if (!is_rotating) {
    return true;
  }
  return false;
}

bool AI::ProcessTalkAction(ObjPtr spider, shared_ptr<TalkAction> action) {
  spider->active_textures[1] = 1;
  spider->active_animation = "Armature|talk";

  // vec3 to_next_location = action->target - spider->position;
  // to_next_location.y = 0;

  // // Check how much time happened since the order was issued.
  // if (glfwGetTime() > action->issued_at + 5) {
  //   return true;
  // }

  // float dist_next_location = length(to_next_location);
  // if (dist_next_location < 7.0f) {
  //   return true;
  // }

  // float speed = spider->GetAsset()->base_speed;
  // bool is_rotating = RotateSpider(spider, action->destination);
  // if (!is_rotating) {
  //   spider->speed += spider->forward * speed;
  // }
  return false;

}

bool AI::ProcessStandAction(ObjPtr spider, shared_ptr<StandAction> action) {
  spider->active_textures[1] = 0;
  spider->active_animation = "Armature|idle";
  return false;
}

bool AI::ProcessMoveAction(ObjPtr spider, shared_ptr<MoveAction> action) {
  spider->active_textures[1] = 0;
  spider->active_animation = "Armature|walking";

  vec3 to_next_location = action->destination - spider->position;
  to_next_location.y = 0;

  // Check how much time happened since the order was issued.
  // if (glfwGetTime() > action->issued_at + 5) {
  //   return true;
  // }

  float dist_next_location = length(to_next_location);
  if (dist_next_location < 7.0f) {
    return true;
  }

  float speed = spider->GetAsset()->base_speed;
  bool is_rotating = RotateSpider(spider, action->destination);
  if (!is_rotating) {
    spider->speed += spider->forward * speed;
  }
  return false;
}

bool AI::ProcessIdleAction(ObjPtr spider, shared_ptr<IdleAction> action) {
  spider->active_animation = "Armature|idle";
  float time_to_end = action->issued_at + action->duration;
  double current_time = glfwGetTime();
  if (current_time > time_to_end) {
    return true;
  }
  return false;
}

// An action should be precise and direct to the point.
void AI::ProcessNextAction(ObjPtr spider) {
  if (spider->actions.empty()) return;

  shared_ptr<Action> action = spider->actions.front();

  switch (action->type) {
    case ACTION_MOVE: {
      shared_ptr<MoveAction> move_action =  
        static_pointer_cast<MoveAction>(action);
      if (ProcessMoveAction(spider, move_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_IDLE: {
      shared_ptr<IdleAction> idle_action =  
        static_pointer_cast<IdleAction>(action);
      if (ProcessIdleAction(spider, idle_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_TAKE_AIM: {
      shared_ptr<TakeAimAction> take_aim_action =  
        static_pointer_cast<TakeAimAction>(action);
      if (ProcessTakeAimAction(spider, take_aim_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_RANGED_ATTACK: {
      shared_ptr<RangedAttackAction> ranged_attack_action =  
        static_pointer_cast<RangedAttackAction>(action);
      if (ProcessRangedAttackAction(spider, ranged_attack_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_CHANGE_STATE: {
      shared_ptr<ChangeStateAction> change_state_action =  
        static_pointer_cast<ChangeStateAction>(action);
      if (ProcessChangeStateAction(spider, change_state_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_STAND: {
      shared_ptr<StandAction> stand_action =  
        static_pointer_cast<StandAction>(action);
      if (ProcessStandAction(spider, stand_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_TALK: {
      shared_ptr<TalkAction> talk_action =  
        static_pointer_cast<TalkAction>(action);
      if (ProcessTalkAction(spider, talk_action)) {
        spider->actions.pop();
        spider->frame = 0;
      }
      break;
    }
    default: 
      break;
  }
}

// TODO: Split AI into actions and intention. If the intention is to hunt, the
// spider may need to take many actions. For example: to wander, a spider
// needs to select a new location, move there, check if it is close to it,
// take another action, check if the player is nearby. Actions also must take 
// the object status into consideration.
void AI::RunSpiderAI() {
  int num_spiders = 0;
  for (ObjPtr obj1 : resources_->GetMovingObjects()) {
    if (obj1->GetAsset()->name != "spider" && 
      obj1->GetAsset()->name != "fisherman-body") continue;

    if (obj1->GetAsset()->name == "spider") {
      num_spiders++;
    }

    ObjPtr spider = obj1;

    // TODO: maybe should go to physics.
    if (dot(spider->up, vec3(0, 1, 0)) < 0) {
      spider->up = vec3(0, 1, 0);
    }

    // Check status. If taking hit, dying, poisoned, etc.
    if (!ProcessStatus(spider)) continue;
    ProcessMentalState(spider);
    ProcessNextAction(spider);
  }

  // TODO: this should go somewhere else.
  if (num_spiders < 10) {
    int dice = rand() % 1000; 
    if (dice > 2) return;

    dice = rand() % 1000; 
    vec3 position = vec3(9649, 134, 10230);
    if (dice < 500) {
      position = vec3(9610, 132, 9885);
    } 

    shared_ptr<GameObject> obj = resources_->CreateGameObjFromAsset(
      "spider", position);
    obj->position = position;
    ChangeState(obj, WANDER);
  }
}

// Objetivo da aranha é se espalhar.

// Só ataca quando está perto do player.


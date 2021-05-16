#include "ai.hpp"
#include <queue>
#include <iomanip>

const float kMinDistance = 500.0f;

AI::AI(shared_ptr<Resources> asset_catalog) : resources_(asset_catalog) {
  CreateThreads();
}

AI::~AI() {
  terminate_ = true;
  for (int i = 0; i < kMaxThreads; i++) {
    ai_threads_[i].join();
  }
}

ivec2 GetDungeonTile(const vec3& position) {
  vec3 offset = vec3(10000 - 5, 300, 10000 - 5);
  ivec2 tile_pos = ivec2(
    (position - offset).x / 10,
    (position - offset).z / 10);

  if (tile_pos.x < 0 || tile_pos.x >= 40 || tile_pos.y < 0 || 
    tile_pos.y >= 40) {
    return ivec2(-1, -1);
  }
  return tile_pos;
}

vec3 GetTilePosition(const ivec2& tile) {
  const vec3 offset = vec3(10000, 300, 10000);
  // return offset + vec3(tile.x, 0, tile.y) * 10.0f + vec3(0.0f, 0, 0.0f);
  return offset + vec3(tile.x, 0, tile.y) * 10.0f;
}

void AI::ChangeState(ObjPtr obj, AiState state) {
  obj->ai_state = state;
  obj->frame = 0;
  obj->state_changed_at = glfwGetTime();
}

ObjPtr AI::GetClosestUnit(ObjPtr spider) {
  float min_distance = 999.0f;
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

  resources_->Lock();
  spider->actions.push(make_shared<TakeAimAction>());
  spider->actions.push(make_shared<RangedAttackAction>());
  resources_->Unlock();
  // spider->actions.push(make_shared<ChangeStateAction>(WANDER));
}

void AI::Chase(ObjPtr spider) {
  if (!spider->actions.empty()) {
    // Complete the actions before choosing other actions.
    return;
  }

  ObjPtr player = resources_->GetObjectByName("player");
  vec3 dir = player->position - spider->position;

  if (spider->GetAsset()->name == "spider" || spider->GetAsset()->name == "demon-vine" || spider->GetAsset()->name == "cephalid" || spider->GetAsset()->name == "metal-eye") {
    int dice = rand() % 3; 
    if (length(dir) > 100 && dice < 2) {
      vec3 next_pos = spider->position + normalize(dir) * 50.0f;
      spider->actions.push(make_shared<MoveAction>(next_pos));
    } else {
      spider->actions.push(make_shared<TakeAimAction>());
      spider->actions.push(make_shared<RangedAttackAction>());
      spider->actions.push(make_shared<IdleAction>(2));
    }
  } else if (spider->GetAsset()->name == "beetle") {
    dir.y = 0;
    vec3 next_pos = spider->position + normalize(dir) * 2.0f;
    spider->actions.push(make_shared<MoveAction>(next_pos));

    if (length(dir) < 10.0f) { 
      spider->actions = {};
      spider->actions.push(make_shared<MeeleeAttackAction>());
    }
  }

  if (glfwGetTime() > spider->state_changed_at + 20) {
    spider->actions = {};
    spider->actions.push(make_shared<ChangeStateAction>(WANDER));
  }
}

void AI::Idle(ObjPtr spider) {
  if (!spider->actions.empty()) {
    // Complete the actions before choosing other actions.
    return;
  }
}

void AI::Wander(ObjPtr spider) {
  if (!spider->actions.empty()) {
    // Complete the actions before choosing other actions.
    return;
  }

  spider->actions.push(make_shared<RandomMoveAction>());
}

bool AI::ProcessStatus(ObjPtr spider) {
  // Check status. If taking hit, or dying.
  switch (spider->status) {
    case STATUS_TAKING_HIT: {
      resources_->ChangeObjectAnimation(spider, "Armature|idle");

      if (spider->frame >= 39) {
        // spider->actions = {};
        // spider->actions.push(make_shared<ChangeStateAction>(CHASE));
        spider->status = STATUS_NONE;
      }
      return false;
    }
    case STATUS_DYING: {
      bool is_dead = false;

      shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
      if (!MeshHasAnimation(*mesh, "Armature|death")) {
        is_dead = true;
      } else {
        resources_->ChangeObjectAnimation(spider, "Armature|death");

        int num_frames = GetNumFramesInAnimation(*mesh, 
          spider->active_animation);
        if (spider->frame >= num_frames - 1) {
          if (spider->GetAsset()->name == "scorpion") {
            if (Random(0, 2) == 0) {
              vec3 position = spider->position + vec3(0, 5.0f, 0);
              ObjPtr obj = CreateGameObjFromAsset(resources_.get(), 
                "open-lock-crystal", position);
              obj->CalculateCollisionData();
            }
          }
          is_dead = true;
        }
      }

      if (is_dead) {
        resources_->CreateParticleEffect(64, spider->position, 
          vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
        spider->status = STATUS_DEAD;
        cout << spider->name << " is dead" << endl;
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

void AI::ProcessNPC(ObjPtr unit) {
  resources_->Lock();
  if (!unit->actions.empty()) {
    resources_->Unlock();
    return;
  }
  resources_->Unlock();

  // unit->LookAt(resources_->GetPlayer()->position);
  resources_->ChangeObjectAnimation(unit, "Armature|idle");
}

// Given the environment and the queued actions, what should be the next 
// actions? 
void AI::ProcessMentalState(ObjPtr unit) {
  if (unit->GetAsset()->type != ASSET_CREATURE) return;

  // IDLE
  // WANDER
  // AGGRESSIVE

  if (unit->IsNpc()) {
    ProcessNPC(unit);
    return;
  }

  // TODO: mental states should be Attack, Flee, Patrol (search), Hunt, Wander.
  switch (unit->ai_state) {
    case IDLE: Idle(unit); break;
    case WANDER: Wander(unit); break;
    case AI_ATTACK: Attack(unit); break;
    case CHASE: Chase(unit); break;
    default: break;
  }
}

bool AI::ProcessRangedAttackAction(ObjPtr spider, 
  shared_ptr<RangedAttackAction> action) {
  if (resources_->GetConfigs()->disable_attacks) {
    return true;
  }

  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  if (int(spider->frame) >= 79) {
    return true;
  }

  if (int(spider->frame) == 44) {
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = (player->position + vec3(0, -3.0f, 0)) - spider->position;

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

    // TODO: change this to make the creature choose the best attack.
    if (spider->GetAsset()->name == "cephalid" || spider->GetAsset()->name == "metal-eye") {
      if (!resources_->SpiderCastPowerMagicMissile(spider, dir)) {
        resources_->SpiderCastMagicMissile(spider, dir);
      }
    } else {
      resources_->SpiderCastMagicMissile(spider, dir);
    }

  }
  return false;
}

bool AI::ProcessMeeleeAttackAction(ObjPtr spider, 
  shared_ptr<MeeleeAttackAction> action) {
  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  if (spider->frame >= 31) {
    return true;
  }

  if (spider->frame == 30) {
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = player->position - spider->position;

    if (length(dir) < 10.0f) {
      player->life -= 10;
      resources_->GetConfigs()->taking_hit = 30.0f;
      if (player->life <= 0.0f) {
        player->life = 100.0f;
        player->position = resources_->GetConfigs()->respawn_point;
      }
    }
    return true;
  }
  return false;
}

bool AI::ProcessCastSpellAction(ObjPtr spider, 
    shared_ptr<CastSpellAction> action) {
  string spell_name = action->spell_name;
  if (spell_name == "burrow") {
    resources_->ChangeObjectAnimation(spider, "Armature|dig");
    if (spider->frame > 4 && spider->frame > 5) {
      resources_->CreateParticleEffect(40, 
        spider->position, 
        vec3(0, 2, 0), vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
    }

    if (spider->frame > 29 && spider->frame > 30) {
      spider->status = STATUS_BURROWED;
      return true;
    }
  } else if (spell_name == "unburrow") {
    resources_->ChangeObjectAnimation(spider, "Armature|dig");
    if (spider->frame > 4 && spider->frame > 5) {
      resources_->CreateParticleEffect(40, 
        spider->position, 
        vec3(0, 2, 0), vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
    }

    if (spider->frame > 29 && spider->frame > 30) {
      spider->status = STATUS_NONE;
      return true;
    }
  }
  return false;
}

bool AI::ProcessRandomMoveAction(ObjPtr spider,
  shared_ptr<RandomMoveAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();

  ivec2 spider_tile = dungeon.GetDungeonTile(spider->position);
  if (!dungeon.IsValidTile(spider_tile)) {
    return true;
  }

  ivec2 next_tile = ivec2(-1, -1);
  for (int i = 0; i < 9; i++) {
    int x = Random(-1, 2);
    int y = Random(-1, 2);

    next_tile = spider_tile + ivec2(x, y);
    if (!dungeon.IsTileClear(next_tile)) continue;
    break;
  }

  if (!dungeon.IsValidTile(next_tile)) {
    return true;
  }

  vec3 next_pos = dungeon.GetTilePosition(next_tile);
  spider->actions.push(make_shared<MoveAction>(next_pos));
  return true;
}

bool AI::ProcessChangeStateAction(ObjPtr spider, 
  shared_ptr<ChangeStateAction> action) {
  ChangeState(spider, action->new_state);
  return true;
}

bool AI::ProcessTakeAimAction(ObjPtr spider, shared_ptr<TakeAimAction> action) {
  resources_->ChangeObjectAnimation(spider, "Armature|walking");

  ObjPtr player = resources_->GetObjectByName("player");
  bool is_rotating = RotateSpider(spider, player->position, 0.99f);
  if (!is_rotating) {
    return true;
  }
  return false;
}

bool AI::ProcessAnimationAction(ObjPtr unit, 
  shared_ptr<AnimationAction> action) {
  if (!resources_->ChangeObjectAnimation(unit, action->animation_name)) {
    return true;
  }

  int num_frames = unit->GetNumFramesInCurrentAnimation();
  if (!action->loop && unit->frame >= num_frames - 1) {
    return true;
  }

  return false;
}

bool AI::ProcessTalkAction(ObjPtr unit, shared_ptr<TalkAction> action) {
  if (!resources_->ChangeObjectAnimation(unit, "Armature|talk")) {
    resources_->ChangeObjectAnimation(unit, "talking");
  }

  int num_frames = unit->GetNumFramesInCurrentAnimation();
  if (unit->frame >= num_frames - 1) {
    return true;
  }
  return false;
}

bool AI::ProcessStandAction(ObjPtr spider, shared_ptr<StandAction> action) {
  if (!resources_->ChangeObjectAnimation(spider, "Armature|idle")) {
    resources_->ChangeObjectAnimation(spider, "idle");
  }
  return false;
}

bool AI::ProcessMoveAction(ObjPtr spider, shared_ptr<MoveAction> action) {
  if (glfwGetTime() > action->issued_at + 3.0f) {
    spider->actions.push(make_shared<RandomMoveAction>());
    return true;
  }

  if (spider->GetType() == ASSET_CREATURE) {
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
  }

  vec3 to_next_location = action->destination - spider->position;

  // TODO: create function GetPhysicsBehavior.
  PhysicsBehavior physics_behavior = spider->physics_behavior;
  if (physics_behavior == PHYSICS_UNDEFINED) {
    if (spider->GetAsset()) {
      physics_behavior = spider->GetAsset()->physics_behavior;
    }
  }

  if (physics_behavior == PHYSICS_FLY || physics_behavior == PHYSICS_SWIM) {

  } else {
    to_next_location.y = 0;
  }

  float dist_next_location = length(to_next_location);
  if (dist_next_location < 2.0f) {
    return true;
  }

  if (spider->GetType() == ASSET_CREATURE) {
    bool is_rotating = RotateSpider(spider, action->destination);
    if (is_rotating) {
      return false;
    } 
  }

  float speed = spider->GetAsset()->base_speed;
  if (physics_behavior == PHYSICS_FLY) {
    spider->speed += normalize(to_next_location) * speed;
  } else {
    spider->speed += spider->forward * speed;
  }
  return false;
}

bool AI::ProcessIdleAction(ObjPtr spider, shared_ptr<IdleAction> action) {
  resources_->ChangeObjectAnimation(spider, "Armature|idle");
  float time_to_end = action->issued_at + action->duration;
  double current_time = glfwGetTime();
  if (current_time > time_to_end) {
    return true;
  }
  return false;
}

bool AI::ProcessMoveToPlayerAction(
  ObjPtr spider, shared_ptr<MoveToPlayerAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();
  const vec3& player_pos = resources_->GetPlayer()->position;
  vec3 next_pos = dungeon.GetNextMove(spider->position, player_pos);
  if (next_pos.x == 0 && next_pos.z == 0) {
    spider->actions.push(make_shared<StandAction>());
    return true;
  }

  spider->actions.push(make_shared<MoveAction>(next_pos));
  return true;
}

bool AI::ProcessMoveAwayFromPlayerAction(
  ObjPtr spider, shared_ptr<MoveAwayFromPlayerAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();

  const vec3& player_pos = resources_->GetPlayer()->position;
  ivec2 player_tile = dungeon.GetDungeonTile(player_pos);
  ivec2 spider_tile = dungeon.GetDungeonTile(spider->position);

  if (!dungeon.IsValidTile(player_tile) || !dungeon.IsValidTile(spider_tile)) {
    return true;
  }

  float max_distance = -999.0f;
  ivec2 best_tile = ivec2(-1, -1);
  for (int x = -1; x < 1; x++) {
    for (int y = -1; y < 1; y++) {
      if (x == 0 && y == 0) continue;

      ivec2 next_tile = spider_tile + ivec2(x, y);
      if (!dungeon.IsTileClear(next_tile)) continue;
     
      float distance = length(vec2(next_tile) - vec2(player_tile));
      if (distance > max_distance) {
        best_tile = next_tile;
        max_distance = distance;
      }
    }
  }

  if (!dungeon.IsValidTile(best_tile)) {
    // TODO: set unit stuck.
    return true;
  }

  vec3 best_pos = dungeon.GetTilePosition(best_tile);
  spider->actions.push(make_shared<MoveAction>(best_pos));
  return true;
}

bool AI::ProcessUseAbilityAction(ObjPtr spider, 
  shared_ptr<UseAbilityAction> action) {
  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 spider_tile = dungeon.GetDungeonTile(spider->position);

  float current_time = glfwGetTime();
  if (spider->cooldowns.find(action->ability) != spider->cooldowns.end()) {
    if( spider->cooldowns["summon-spiderling"] > current_time) {
      return true;
    }
  }

  // TODO: maybe call script with ability effect.
  if (action->ability == "summon-spiderling") {
    int num_summons = 2;
    for (int i = 0; i < 100; i++) {
      int off_x = Random(-2, 2);
      int off_y = Random(-2, 2);
      ivec2 tile = spider_tile + ivec2(off_x, off_y);
      if (!dungeon.IsTileClear(tile)) continue;
     
      vec3 tile_pos = dungeon.GetTilePosition(tile);
      ObjPtr spiderling = CreateGameObjFromAsset(resources_.get(), "spiderling", tile_pos);
      spiderling->ai_state = AI_ATTACK;
      resources_->CreateParticleEffect(32, tile_pos, 
        vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
      if (--num_summons == 0) break;
    }
    spider->cooldowns["summon-spiderling"] = glfwGetTime() + 15;
  } else if (action->ability == "spider-web") {
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = (player->position + vec3(0, -3.0f, 0)) - spider->position;

    vec3 forward = normalize(vec3(dir.x, 0, dir.z));
    vec3 pos = spider->position;
    for (int i = 0; i < 5; i++) {
      ObjPtr web = CreateGameObjFromAsset(resources_.get(), "spider-web", pos);
      web->life = 100.0f;
      pos += forward * 10.0f;
    }
    spider->cooldowns["spider-web"] = glfwGetTime() + 5;
  }
  return true;
}

// An action should be precise and direct to the point.
void AI::ProcessNextAction(ObjPtr spider) {
  if (spider->actions.empty()) return;

  resources_->Lock();
  shared_ptr<Action> action = spider->actions.front();
  resources_->Unlock();

  switch (action->type) {
    case ACTION_MOVE: {
      shared_ptr<MoveAction> move_action =  
        static_pointer_cast<MoveAction>(action);
      if (ProcessMoveAction(spider, move_action)) {

        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = move_action;
        resources_->Unlock();

        shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
          int num_frames = GetNumFramesInAnimation(*mesh, 
            spider->active_animation);
        if (spider->frame >= num_frames) {
          spider->frame = 0;
        }
      }
      break;
    }
    case ACTION_RANDOM_MOVE: {
      shared_ptr<RandomMoveAction> random_move_action =  
        static_pointer_cast<RandomMoveAction>(action);
      if (ProcessRandomMoveAction(spider, random_move_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = random_move_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_IDLE: {
      shared_ptr<IdleAction> idle_action =  
        static_pointer_cast<IdleAction>(action);
      if (ProcessIdleAction(spider, idle_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = idle_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_TAKE_AIM: {
      shared_ptr<TakeAimAction> take_aim_action =  
        static_pointer_cast<TakeAimAction>(action);
      if (ProcessTakeAimAction(spider, take_aim_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = take_aim_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_MEELEE_ATTACK: {
      shared_ptr<MeeleeAttackAction> meelee_attack_action =  
        static_pointer_cast<MeeleeAttackAction>(action);
      if (ProcessMeeleeAttackAction(spider, meelee_attack_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = meelee_attack_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_MOVE_TO_PLAYER: {
      shared_ptr<MoveToPlayerAction> move_to_player_action =  
        static_pointer_cast<MoveToPlayerAction>(action);
      if (ProcessMoveToPlayerAction(spider, move_to_player_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = move_to_player_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_MOVE_AWAY_FROM_PLAYER: {
      shared_ptr<MoveAwayFromPlayerAction> move_away_from_player_action =  
        static_pointer_cast<MoveAwayFromPlayerAction>(action);
      if (ProcessMoveAwayFromPlayerAction(spider, move_away_from_player_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = move_away_from_player_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_RANGED_ATTACK: {
      shared_ptr<RangedAttackAction> ranged_attack_action =  
        static_pointer_cast<RangedAttackAction>(action);
      if (ProcessRangedAttackAction(spider, ranged_attack_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = ranged_attack_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_CAST_SPELL: {
      shared_ptr<CastSpellAction> cast_spell_action =  
        static_pointer_cast<CastSpellAction>(action);
      if (ProcessCastSpellAction(spider, cast_spell_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = cast_spell_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_CHANGE_STATE: {
      shared_ptr<ChangeStateAction> change_state_action =  
        static_pointer_cast<ChangeStateAction>(action);
      if (ProcessChangeStateAction(spider, change_state_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = change_state_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_STAND: {
      shared_ptr<StandAction> stand_action =  
        static_pointer_cast<StandAction>(action);
      if (ProcessStandAction(spider, stand_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = stand_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_TALK: {
      shared_ptr<TalkAction> talk_action =  
        static_pointer_cast<TalkAction>(action);
      if (ProcessTalkAction(spider, talk_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = talk_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_ANIMATION: {
      shared_ptr<AnimationAction> animation_action =  
        static_pointer_cast<AnimationAction>(action);
      if (ProcessAnimationAction(spider, animation_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = animation_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    case ACTION_USE_ABILITY: {
      shared_ptr<UseAbilityAction> use_ability_action =  
        static_pointer_cast<UseAbilityAction>(action);
      if (ProcessUseAbilityAction(spider, use_ability_action)) {
        resources_->Lock();
        spider->actions.pop();
        spider->prev_action = use_ability_action;
        resources_->Unlock();
        spider->frame = 0;
      }
      break;
    }
    default: {
      break;
    }
  }
}

void AI::ProcessPlayerAction(ObjPtr player) {
  if (player->actions.empty()) return;

  shared_ptr<Action> action = player->actions.front();
  float player_speed = resources_->GetConfigs()->player_speed; 
  shared_ptr<Configs> configs = resources_->GetConfigs();

  float d = resources_->GetDeltaTime() / 0.016666f;
  switch (action->type) {
    case ACTION_MOVE: {
      shared_ptr<MoveAction> move_action =  
        static_pointer_cast<MoveAction>(action);
      vec3 to_next_location = move_action->destination - player->position;

      shared_ptr<Player> player_p = static_pointer_cast<Player>(player);
      player_p->LookAt(to_next_location + vec3(0, 5.5, 0));

      to_next_location.y = 0;

      float dist_next_location = length(to_next_location);
      if (dist_next_location < 1.0f) {
        player->actions.pop();
      } else {
        player->speed += normalize(to_next_location) * player_speed * d;
      }
      break;
    }
    case ACTION_WAIT: {
      shared_ptr<WaitAction> wait_action =  
        static_pointer_cast<WaitAction>(action);
      if (glfwGetTime() > wait_action->until) {
        player->actions.pop();
      }
      break;
    }
    case ACTION_TALK: {
      shared_ptr<TalkAction> talk_action =  
        static_pointer_cast<TalkAction>(action);
      resources_->TalkTo(talk_action->npc);
      player->actions.pop();
      break;
    }
    case ACTION_LOOK_AT: {
      shared_ptr<LookAtAction> look_at_action =  
        static_pointer_cast<LookAtAction>(action);
      ObjPtr target_obj = resources_->GetObjectByName(look_at_action->obj);
      if (target_obj) {
        vec3 to_obj = target_obj->position - player->position;
        if (configs->override_camera_pos) {
          to_obj = target_obj->position - configs->camera_pos;
        }

        if (target_obj->IsNpc()) {
          to_obj = target_obj->position + vec3(0, 5.5, 0) - player->position;
        }
        shared_ptr<Player> player_p = static_pointer_cast<Player>(player);
        player_p->LookAt(to_obj);
      }
      player->actions.pop();
      break;
    }
    default: 
      break;
  }
}

void AI::RunAiInOctreeNode(shared_ptr<OctreeNode> node) {
  if (!node) return;

  const vec3& player_pos = resources_->GetPlayer()->position;
  for (int i = 0; i < 3; i++) {
    if ((player_pos[i] - node->center[i]) > 
      node->half_dimensions[i] + kMinDistance) {
      return;
    }
  }

  resources_->Lock();
  for (auto [id, obj] : node->moving_objs) {
    if (obj->type != GAME_OBJ_DEFAULT) continue;
    if (obj->GetAsset()->type != ASSET_CREATURE && 
      obj->GetAsset()->type != ASSET_PLATFORM) continue;
    if (obj->being_placed) continue;
    if (obj->distance > kMinDistance) continue;
    ai_mutex_.lock();
    ai_tasks_.push(obj);
    ai_mutex_.unlock();
  }
  resources_->Unlock();
  
  for (int i = 0; i < 8; i++) {
    RunAiInOctreeNode(node->children[i]);
  }
}

void AI::Run() {
  RunAiInOctreeNode(resources_->GetOctreeRoot());
  ProcessPlayerAction(resources_->GetPlayer());

  resources_->GetDungeon().CalculateVisibility(
    resources_->GetPlayer()->position);

  while (!ai_tasks_.empty() || running_tasks_ > 0) {
    this_thread::sleep_for(chrono::microseconds(200));
  }
  return;

  // Sync.
  while (!ai_tasks_.empty()) {
    auto& obj = ai_tasks_.front();
    ai_tasks_.pop();
    running_tasks_++;
    ai_mutex_.unlock();

    // TODO: maybe should go to physics.
    // if (dot(obj->up, vec3(0, 1, 0)) < 0) {
    //   obj->up = vec3(0, 1, 0);
    // }

    // Check status. If taking hit, dying, poisoned, etc.
    if (ProcessStatus(obj)) {
      // ProcessMentalState(obj);
      ProcessNextAction(obj);
    }

    string ai_script = obj->GetAsset()->ai_script;
    if (!ai_script.empty()) {
      resources_->CallStrFn(ai_script, obj->name);
    }

    ai_mutex_.lock();
    running_tasks_--;
    ai_mutex_.unlock();
  }
}

void AI::ProcessUnitAiAsync() {
  while (!terminate_) {
    ai_mutex_.lock();
    if (ai_tasks_.empty()) {
      ai_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(1));
      continue;
    }

    auto& obj = ai_tasks_.front();
    ai_tasks_.pop();
    running_tasks_++;
    ai_mutex_.unlock();

    // TODO: maybe should go to physics.
    // if (dot(obj->up, vec3(0, 1, 0)) < 0) {
    //   obj->up = vec3(0, 1, 0);
    // }

    // Check status. If taking hit, dying, poisoned, etc.
    if (ProcessStatus(obj)) {
      // ProcessMentalState(obj);
      ProcessNextAction(obj);
    }

    string ai_script = obj->GetAsset()->ai_script;
    if (!ai_script.empty()) {
      resources_->CallStrFn(ai_script, obj->name);
    }

    ai_mutex_.lock();
    running_tasks_--;
    ai_mutex_.unlock();
  }
}

void AI::CreateThreads() {
  for (int i = 0; i < kMaxThreads; i++) {
    ai_threads_.push_back(thread(&AI::ProcessUnitAiAsync, this));
  }
}

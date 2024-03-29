#include "ai.hpp"
#include <queue>
#include <iomanip>

const float kMinDistance = 500.0f;

AI::AI(shared_ptr<Resources> resources, shared_ptr<Monsters> monsters) 
  : resources_(resources), monsters_(monsters) {
  CreateThreads();
}

AI::~AI() {
  terminate_ = true;
  for (int i = 0; i < kMaxThreads; i++) {
    ai_threads_[i].join();
  }
}

vec3 GetTilePosition(const ivec2& tile) {
  const vec3 offset = vec3(10000, 300, 10000);
  // return offset + vec3(tile.x, 0, tile.y) * 10.0f + vec3(0.0f, 0, 0.0f);
  return offset + vec3(tile.x, 0, tile.y) * 10.0f;
}

void AI::ChangeState(ObjPtr obj, AiState state) {
  obj->ai_state = state;
  // obj->frame = 0;
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
 
  // At rest position, the object forward direction is +Z and up is +Y.
  spider->forward = normalize(to_point);
  quat h_rotation = RotationBetweenVectors(front, spider->forward);
  // quat v_rotation = RotationBetweenVectors(up, spider->up);
  // quat target_rotation = v_rotation * h_rotation;
  quat target_rotation = h_rotation;

  // Check if object is facing the correct direction.
  vec3 cur_forward = spider->rotation_matrix * front;
  cur_forward.y = 0;
  quat cur_h_rotation = RotationBetweenVectors(front, 
    normalize(cur_forward));

  bool is_rotating = abs(dot(h_rotation, cur_h_rotation)) < rotation_threshold;
  spider->cur_rotation =  
    RotateTowards(spider->cur_rotation, target_rotation, turn_rate);
  spider->rotation_matrix = mat4_cast(spider->cur_rotation);
  return is_rotating; 
}

bool AI::RotateSpiderWithStrafe(ObjPtr spider, vec3 point, int* move_type,
  float rotation_threshold) {
  vec3 to_point = point - spider->position;
  to_point.y = 0;

  const vec3 front = vec3(0, 0, 1);
  const vec3 right = vec3(1, 0, 0);
  const vec3 left  = vec3(-1, 0, 0);
 
  // Check if object is facing the correct direction.
  vec3 cur_forward = spider->rotation_matrix * front;
  cur_forward.y = 0;

  quat cur_front_rotation = RotationBetweenVectors(front, 
    normalize(cur_forward));

  // At rest position, the object forward direction is +Z and up is +Y.
  spider->forward = normalize(to_point);

  quat target_front_rotation = RotationBetweenVectors(front, spider->forward);
  quat target_right_rotation = RotationBetweenVectors(right, spider->forward);
  quat target_left_rotation = RotationBetweenVectors(left, spider->forward);

  // Check which is better: front, strafe left or strafe right.
  float front_magnitude = abs(dot(target_front_rotation, cur_front_rotation));
  float right_magnitude = abs(dot(target_right_rotation, cur_front_rotation));
  float left_magnitude = abs(dot(target_left_rotation, cur_front_rotation));

  float best_magnitude = 0.0f;
  quat target_rotation;

  // Front.
  if (front_magnitude > right_magnitude && front_magnitude > left_magnitude) {
    best_magnitude = front_magnitude;
    target_rotation = target_front_rotation;
    *move_type = 0;
  // Right.
  } else if (right_magnitude > left_magnitude) {
    best_magnitude = right_magnitude;
    target_rotation = target_right_rotation;
    *move_type = 1;
  // Left.
  } else {
    best_magnitude = left_magnitude;
    target_rotation = target_left_rotation;
    *move_type = 2;
  } 

  float turn_rate = spider->GetAsset()->base_turn_rate;

  bool is_rotating = best_magnitude < rotation_threshold;
  spider->cur_rotation =  
    RotateTowards(spider->cur_rotation, target_rotation, turn_rate);
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

void AI::Idle(ObjPtr unit) {
  resources_->ChangeObjectAnimation(unit, "Armature|idle");
  if (!unit->actions.empty()) {
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
  unordered_map<int, ItemData>& item_data = resources_->GetItemData();
  Dungeon& dungeon = resources_->GetDungeon();

  // Check status. If taking hit, or dying.
  switch (spider->status) {
    case STATUS_TAKING_HIT: {
      // resources_->ChangeObjectAnimation(spider, "Armature|idle");
      resources_->ChangeObjectAnimation(spider, "Armature|taking_hit", 
        false, TRANSITION_NONE);

      shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
      int num_frames = GetNumFramesInAnimation(*mesh, "Armature|taking_hit");

      if (spider->frame >= 35 || spider->frame >= num_frames - 5) {
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
        resources_->ChangeObjectAnimation(spider, "Armature|death", false, 
          TRANSITION_NONE);

        // TODO: move drop logic to another class.
        int num_frames = GetNumFramesInAnimation(*mesh, "Armature|death");
        if (spider->frame >= num_frames - 2) {
          resources_->CreateDrops(spider);
          is_dead = true;
        }
      }

      if (is_dead) {
        // resources_->CreateParticleEffect(64, spider->position, 
        //   vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
        if (spider->GetAsset()->name == "skeleton" ||
            spider->GetAsset()->name == "wraith") {
          if (spider->ai_state != IDLE) {
            ObjPtr skull = CreateGameObjFromAsset(resources_.get(), "horse_skull", 
              spider->position);
            float x = Random(0, 11) * .05f;
            float z = Random(0, 11) * .05f;
            skull->speed = spider->speed;
            skull->torque = cross(normalize(skull->speed), vec3(1, 1, 0)) * 5.0f;
            spider->ai_state = IDLE;
            spider->ClearActions();
            spider->cooldowns["rebirth"] = glfwGetTime() + 10.0f;
            spider->created_obj = skull;
            spider->always_cull = true;
            spider->life = 100.0f;
            spider->status = STATUS_NONE;
          }
        } else {
          spider->status = STATUS_DEAD;
        }
      }
      return false;
    }
    case STATUS_DEAD: {
      shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
      if (MeshHasAnimation(*mesh, "Armature|death")) {
        resources_->ChangeObjectAnimation(spider, "Armature|death");
      }

      int num_frames = GetNumFramesInAnimation(*mesh, spider->active_animation);
      spider->frame = num_frames - 1;
      return false;
    }
    case STATUS_STUN: {
      resources_->ChangeObjectAnimation(spider, "Armature|idle");
      return false;
    }
    case STATUS_NONE:
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

bool AI::ProcessRangedAttackAction(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  if (!creature->CanUseAbility("ranged-attack")) return true;

  if (resources_->GetConfigs()->disable_attacks) {
    return true;
  }

  if (creature->GetAsset()->name == "white_spine") {
    return WhiteSpineAttack(creature, action);
  } else if (creature->GetAsset()->name == "scorpion") {
    return ScorpionAttack(creature, action);
  } else if (creature->GetAsset()->name == "shooter_bug") {
    return ShooterBugAttack(creature, action);
  } else if (creature->GetAsset()->name == "red_metal_eye") {
    return RedMetalEyeAttack(creature, action);
  } else if (creature->GetAsset()->name == "metal_eye") {
    return MetalEyeAttack(creature, action);
  } else if (creature->GetAsset()->name == "imp") {
    return ImpAttack(creature, action);
  } else if (creature->GetAsset()->name == "beholder") {
    return BeholderAttack(creature, action);
  } else if (creature->GetAsset()->name == "big_beholder") {
    return BigBeholderAttack(creature, action);
  } else if (creature->GetAsset()->name == "skirmisher") {
    return SkirmisherAttack(creature, action);
  } else if (creature->GetAsset()->name == "glaive_master") {
    return GlaiveMasterAttack(creature, action);
  } else if (creature->GetAsset()->name == "black_mage_body") {
    return BlackMageAttack(creature, action);
  } else if (creature->GetAsset()->name == "little_stag" ||
             creature->GetAsset()->name == "little_stag_shadow") {
    return LittleStagAttack(creature, action);
  }

  if (creature->GetAsset()->name == "wraith") {
    return WraithAttack(creature, action);
  }

  return true;
}

bool AI::ProcessDefendAction(ObjPtr creature, shared_ptr<DefendAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|defend", 
    false, TRANSITION_NONE);

  if (!action->started) {
    if (!creature->CanUseAbility("defend")) return true;
    action->started = true;
    action->until = glfwGetTime() + 0.5f;

    if (creature->GetAsset()->name == "goblin_chieftain") {
      creature->cooldowns["defend"] = glfwGetTime() + 1.5;
    } else {
      creature->cooldowns["defend"] = glfwGetTime() + 2;
    }

    creature->AddTemporaryStatus(make_shared<InvulnerableStatus>(1.0f, 20.0f));
  } else if (action->started && creature->frame > 35) {
    return true;
  }
  return false;
}

vec3 AI::FindSideMove(ObjPtr unit) {
  Dungeon& dungeon = resources_->GetDungeon();
  shared_ptr<Player> player = resources_->GetPlayer();
  vec3 center = player->position;

  ivec2 player_tile_pos = dungeon.GetDungeonTile(player->position);
  ivec2 unit_tile_pos = dungeon.GetDungeonTile(unit->position);

  ivec2 step;
  step.x = sign(player_tile_pos.x - unit_tile_pos.x);
  step.y = sign(player_tile_pos.y - unit_tile_pos.y);

  ivec2 next_tile;
  if (Random(0, 2) == 0) {
    next_tile = ivec2(step.y, -step.x);
  } else {
    next_tile = ivec2(-step.y, step.x);
  }

  if (!dungeon.IsValidTile(unit_tile_pos + next_tile) ||
      !dungeon.IsTileClear(unit_tile_pos + next_tile)) {
    next_tile = -next_tile;
  }

  if (!dungeon.IsValidTile(unit_tile_pos + next_tile) ||
      !dungeon.IsTileClear(unit_tile_pos + next_tile)) {
    return vec3(0);
  }

  return dungeon.GetTilePosition(unit_tile_pos + next_tile);
}

bool AI::ProcessSideStepAction(ObjPtr creature, 
  shared_ptr<SideStepAction> action) {
  if (!creature->can_jump) {
    return true;
  }

  vec3 pos = FindSideMove(creature);
  if (length2(pos) <= 0.1f) {
    return true;
  }

  creature->frame = 0;
  resources_->ChangeObjectAnimation(creature, "Armature|walking");
  vec3 v = pos - creature->position;
  v.y = 0;

  creature->speed += normalize(v) * 0.3f + vec3(0, 0.2f, 0);
  creature->can_jump = false;
  return false;
}

bool AI::ProcessTeleportAction(ObjPtr creature, 
  shared_ptr<TeleportAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|walking");

  if (!action->started) {
    creature->cooldowns["teleport"] = glfwGetTime() + 10;
    action->started = true;
    action->channel_until = glfwGetTime() + 0.5;
  }

  if (glfwGetTime() < action->channel_until) {
    resources_->CreateParticleEffect(1, creature->position + vec3(0, 3, 0), 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 1.0, 24.0f, 15.0f, "fireball");          
  } else {
    creature->position = action->position;
    resources_->CreateParticleEffect(10, creature->position + vec3(0, 3, 0), 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 5.0, 24.0f, 15.0f, "fireball");          
    return true;
  }
  return false;
}

bool AI::ProcessFireballAction(ObjPtr creature, 
  shared_ptr<FireballAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 40) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -6.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  if (creature->frame >= 26 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 dir = CalculateMissileDirectionToHitTarget(creature->position, target,
      4.0f);

    cout << "dir: " << dir << endl;

    resources_->CastFireball(creature, dir);
    creature->cooldowns["fireball"] = glfwGetTime() + 10;
  }
  return false;
}

bool AI::ImpAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 59) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  BoundingSphere s = creature->GetBoneBoundingSphere(48);
  vec3 pos = s.center;

  if (creature->frame >= 26 && !action->damage_dealt) {
    action->damage_dealt = true;
   
    vec3 dir = normalize(target - pos);
    resources_->CastImpFire(creature, pos, dir);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.0;
  }
  return false;
}

bool AI::BeholderAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  static const vector<int> bones { 10, 18, 6, 14, };

  if (!action->initiated) {
    creature->hit_list.clear();
    bool is_rotating = RotateSpider(creature, target, 0.01f);
    if (is_rotating) return false;

    action->until = 60.0f;
    action->initiated = true;

    for (int i = 0; i < 4; i++) {
      BoundingSphere bs = creature->GetBoneBoundingSphere(bones[i]);
     
      shared_ptr<Particle> p = resources_->CreateOneParticle(bs.center, 60.0f, 
        "particle-sparkle-fire", 5.0f);
      p->associated_obj = creature;
      p->offset = vec3(0);
      p->associated_bone = bones[i];
    }
    action->initiated = true;
  }

  action->until -= 1.0f;
  if (action->until <= 0.0f) return true;

  BoundingSphere s = creature->GetBoneBoundingSphere(0);
  vec3 pos = s.center;

  vec3 v = creature->forward * (0.1f + float(60 - action->until) / 2.0f) - vec3(0, 3, 0);
  for (int i = 0; i < 4; i++) {
    float random_noise = Random(-5, 6) * 0.01f;
    vec3 v2 = rotate(v, random_noise, vec3(0, 1, 0));
    if (i < 2) v2.y += 0.2f;

    resources_->CastMagmaRay(creature, creature->position, normalize(v2), bones[i]);
  }
  
  return false;
}

bool AI::BigBeholderAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 40) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -6.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  shared_ptr<Mesh> mesh = resources_->GetMesh(creature);
  int bone_id = mesh->bones_to_ids["head"];
  BoundingSphere s = creature->GetBoneBoundingSphere(bone_id);
  vec3 pos = s.center;

  if (creature->frame >= 26 && !action->damage_dealt) {
    action->damage_dealt = true;
   
    // vec3 dir = normalize(target - pos);
    // resources_->CastParalysis(creature, target);
    // creature->cooldowns["paralysis"] = glfwGetTime() + 1.5;

    pos.x += Random(-5, 6) * 1.0f;
    pos.z += Random(-5, 6) * 1.0f;
    for (int i = 0; i < 4; i++) {
      ObjPtr shadow = CreateGameObjFromAsset(resources_.get(), 
        "beholder_eye", pos);
      creature->created_shadows.push_back(shadow);
      shadow->ai_state = AI_ATTACK;

      vec3 dir = normalize(creature->position - player->position);
      vec3 dir_ = dir;
      float spread = 0.1f;
      float x_ang = (float) Random(-5, 6) * spread;
      float y_ang = (float) Random(-3, 4) * spread;
      vec3 right = cross(dir, vec3(0, 1, 0));
      mat4 m = rotate(mat4(1.0f), x_ang, vec3(0, 1, 0));
      dir_ = vec3(rotate(m, y_ang, right) * vec4(dir, 1.0f));
      shadow->speed += dir_ * 3.0f;
    }
  }
  return false;
}

bool AI::ProcessParalysisAction(ObjPtr creature, 
  shared_ptr<ParalysisAction> action) {

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 40) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -6.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  BoundingSphere s = creature->GetBoneBoundingSphere(1);
  vec3 pos = s.center;

  if (creature->frame >= 26 && !action->damage_dealt) {
    action->damage_dealt = true;
   
    vec3 dir = normalize(target - pos);
    resources_->CastParalysis(creature, target);
    creature->cooldowns["paralysis"] = glfwGetTime() + 1.5;
  }
  return false;
}

bool AI::ProcessFlyLoopAction(ObjPtr creature, 
  shared_ptr<FlyLoopAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|walking");
  ObjPtr player = resources_->GetPlayer();

  bool is_rotating = RotateSpider(creature, player->position, 0.95f);
  if (!action->started) {
    action->circle_front = player->position - creature->position;
    action->circle_front.y = 0;
    action->circle_front = normalize(action->circle_front);

    vec3 dir = vec3(0, action->circle_radius, 0);
    if (creature->position.y > 45.0f) {
      dir = vec3(0, -action->circle_radius, 0);
    } else if (creature->position.y < 22.0f) {
      dir = vec3(0, action->circle_radius, 0);
    } else {
      float ang = ((float) Random(0, 101) / 100.0f) * 2 * 3.1416f;
      mat4 m = rotate(mat4(1.0f), ang, action->circle_front);
      dir = vec3(m * vec4(dir, 1.0f));
    }

    action->circle_center = creature->position + dir;
    action->circle_center.y = std::min(45.0f, action->circle_center.y);
    action->circle_center.y = std::max(22.0f, action->circle_center.y);
    
    action->until = glfwGetTime() + action->duration;
    action->right = Random(0, 2) == 0;
    action->started = true;
  }

  vec3 to_circle_center = action->circle_center - creature->position;
  to_circle_center -= dot(to_circle_center, action->circle_front) * action->circle_front;
  vec3 n_to_circle_center = normalize(to_circle_center);

  // vec3 speed_vector = normalize(cross(n_to_circle_center, action->circle_front)) *
  //   creature->current_speed;
  vec3 speed_vector = normalize(cross(n_to_circle_center, action->circle_front));
  speed_vector *= creature->current_speed;

  creature->speed += speed_vector;

  float radius = length(to_circle_center);

  float magnitude = (action->circle_radius - radius) / action->circle_radius;
  creature->speed += -n_to_circle_center * magnitude * creature->current_speed;

  vec3 to_player = player->position - creature->position;
  to_player.y = 0;
  float l = length(to_player);
  if (l < 60.0f) {
    creature->speed -= normalize(to_player) * creature->current_speed * 2.0f;
  } else if (l > 100.0f) {
    creature->speed += normalize(to_player) * creature->current_speed * 2.0f;
  }

  if (glfwGetTime() > action->until) {
    return true;
  }
  return false;
}

bool AI::ProcessSpinAction(ObjPtr creature, 
  shared_ptr<SpinAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();
  resources_->ChangeObjectAnimation(creature, "Armature|spin");

  vec3 v = action->target - creature->position;
  v.y = 0;
 
  if (length(v) < 5.0f) {
    creature->speed *= 0.3f;
    return true;
  }

  creature->speed = normalize(v) * 2.0f;
  return false;
}

bool AI::ProcessMirrorImageAction(ObjPtr creature, 
  shared_ptr<MirrorImageAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();
  resources_->ChangeObjectAnimation(creature, "Armature|spin");

  if (!creature->created_shadows.empty()) {
    for (ObjPtr shadow : creature->created_shadows) {
      resources_->RemoveObject(shadow);
    }
    creature->created_shadows.clear();
  }
  std::cout << "Mirror image" << endl;

  vec3 pos = creature->position; 
  pos.x += Random(-5, 6) * 1.0f;
  pos.z += Random(-5, 6) * 1.0f;
  for (int i = 0; i < 4; i++) {
    ObjPtr shadow = CreateGameObjFromAsset(resources_.get(), "little_stag_shadow", pos);
    creature->created_shadows.push_back(shadow);
    shadow->ai_state = AMBUSH;
  }

  resources_->CreateParticleEffect(10, creature->position,
    vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 3.0, 24.0f, 15.0f, "fireball");          

  return true;
}

bool AI::WhiteSpineAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const int num_missiles = boost::lexical_cast<int>(resources_->GetGameFlag("white_spine_num_missiles"));
  const float missile_speed = boost::lexical_cast<float>(resources_->GetGameFlag("white_spine_missile_speed"));
  const float spread = boost::lexical_cast<float>(resources_->GetGameFlag("white_spine_missile_spread"));

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 58) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  if (creature->frame >= 42 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 dir = CalculateMissileDirectionToHitTarget(creature->position, target,
      missile_speed);

    for (int i = 0; i < num_missiles; i++) {
      vec3 p2 = creature->position + dir * 200.0f;
      vec3 dir_ = dir;

      if (i > 0) {
        float x_ang = (float) Random(-5, 6) * spread;
        float y_ang = (float) Random(-5, 6) * spread;
        vec3 right = cross(dir, vec3(0, 1, 0));
        mat4 m = rotate(mat4(1.0f), x_ang, vec3(0, 1, 0));
        dir_ = vec3(rotate(m, y_ang, right) * vec4(dir, 1.0f));
      }

      resources_->CastMissile(creature, creature->position, MISSILE_HORN, dir_, 
        missile_speed);
    }
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;
  }
  return false;
}

bool AI::SkirmisherAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const float missile_speed = 3.0f;

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 58) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  if (creature->frame >= 42 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 dir = target - creature->position + vec3(0, 3, 0);
    dir.y = 0;
    dir = normalize(dir);

    resources_->CastMissile(creature, creature->position + vec3(0, 3, 0),  
      MISSILE_GLAIVE, dir, missile_speed);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;
  }
  return false;
}

bool AI::GlaiveMasterAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const float missile_speed = 3.0f;
  const int num_missiles = 5;
  const float spread = 0.25;

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 58) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  if (creature->frame >= 42 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 dir = target - creature->position + vec3(0, 3, 0);
    dir.y = 0;
    dir = normalize(dir);

    resources_->CastMissile(creature, creature->position + vec3(0, 3, 0),  
      MISSILE_GLAIVE, dir, missile_speed);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 2.5;

    for (int i = 0; i < num_missiles; i++) {
      vec3 p2 = creature->position + dir * 200.0f;
      vec3 dir_ = dir;

      if (i > 0) {
        float x_ang = (float) Random(-5, 6) * spread;
        vec3 right = cross(dir, vec3(0, 1, 0));
        mat4 m = rotate(mat4(1.0f), x_ang, vec3(0, 1, 0));
        dir_ = vec3(m * vec4(dir, 1.0f));
      }

      resources_->CastMissile(creature, creature->position + vec3(0, 3, 0),  
        MISSILE_GLAIVE, dir_, missile_speed);
    }
  }
  return false;
}

bool AI::LittleStagAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 58) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  if (creature->frame >= 42 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 dir = target - (creature->position + vec3(0, 3, 0));
    dir.y = 0;
    dir = normalize(dir);

    if (creature->GetAsset()->name == "little_stag") {
      resources_->CastShotgun(creature, creature->position + vec3(0, 3, 0), dir);
    }
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;
  }
  return false;
}

bool AI::BlackMageAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const float missile_speed = 0.5f;

  if (!action->initiated) {
    action->initiated = true;
    action->until = glfwGetTime() + 1.0f;
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
  }

  if (glfwGetTime() > action->until) {
    resources_->ChangeObjectAnimation(creature, "Armature|attack");
  } else {
    resources_->CreateParticleEffect(1, creature->position, 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 1.0, 24.0f, 15.0f, "fireball");          
    return false;
  }

  if (int(creature->frame) >= 58) return true;

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position + vec3(0, -3.0f, 0);
  if (length2(action->target) > 0.1f) {
    target = action->target;
  }

  if (creature->frame >= 42 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 dir = target - creature->position + vec3(0, 3, 0);
    dir.y = 0;
    dir = normalize(dir);

    resources_->CastMissile(creature, creature->position + vec3(0, 3, 0),  
      MISSILE_IMP_FIRE, dir, missile_speed);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 3.0;
  }
  return false;
}

bool AI::ScorpionAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const int num_missiles = 1;
  const float missile_speed = 1.2f + Random(0, 5) * 0.3f;

  ObjPtr target_unit = creature->GetCurrentTarget();

  if (!action->initiated) {
    action->initiated = true;
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
  }

  bool is_rotating = RotateSpider(creature, target_unit->position, 0.8f);
  if (is_rotating) {
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
    return false;
  }

  resources_->ChangeObjectAnimation(creature, "Armature|attack", true, TRANSITION_NONE);

  int num_frames = creature->GetNumFramesInCurrentAnimation();
  if (int(creature->frame) >= num_frames - 3) {
    return true;
  }

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position;

  // Predict player pos.
  if (length(player->speed) > 0.01) {
    // vec2 target_pos = PredictMissileHitLocation(vec2(creature->position.x, creature->position.z), 
    //   missile_speed, vec2(player->position.x, player->position.z), 
    //   vec2(player->speed.x, player->speed.z), 
    //   length(player->speed));
    vec2 target_pos = vec2(player->position.x, player->position.z);
    target = vec3(target_pos.x, player->position.y, target_pos.y);
  }

  RotateSpider(creature, target, 0.99f);

  vec3 dir = normalize(target - creature->position);
  dir.y = 0.1f;

  if (int(creature->frame) >= 41 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 p2 = creature->position + dir * 200.0f;
    resources_->CastMissile(creature, creature->position + vec3(0, 2, 0), 
      MISSILE_BOUNCYBALL, dir, missile_speed);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;

    shared_ptr<Mesh> mesh = resources_->GetMesh(creature);
    int bone_id = mesh->bones_to_ids["muzzle_bone"];
    BoundingSphere s = creature->GetBoneBoundingSphere(bone_id);
    resources_->CreateParticleEffect(10, s.center,
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 3.0, 24.0f, 15.0f, "fireball");          
  }
  return false;
}

bool AI::ShooterBugAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const int num_missiles = 1;
  const float missile_speed = 1.2f + Random(0, 5) * 0.3f;

  ObjPtr target_unit = creature->GetCurrentTarget();

  if (!action->initiated) {
    action->initiated = true;
    resources_->ChangeObjectAnimation(creature, "Armature|walking", true,
      TRANSITION_FINISH_ANIMATION);
  }

  bool is_rotating = RotateSpider(creature, target_unit->position, 0.8f);
  if (is_rotating) {
    resources_->ChangeObjectAnimation(creature, "Armature|walking", true,
      TRANSITION_FINISH_ANIMATION);
    return false;
  }

  resources_->ChangeObjectAnimation(creature, "Armature|attack", true, TRANSITION_NONE);

  int num_frames = creature->GetNumFramesInCurrentAnimation();
  if (int(creature->frame) >= num_frames - 3) {
    return true;
  }

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position;

  // Predict player pos.
  if (length(player->speed) > 0.01) {
    vec2 target_pos = PredictMissileHitLocation(vec2(creature->position.x, creature->position.z), 
      missile_speed, vec2(player->position.x, player->position.z), 
      vec2(player->speed.x, player->speed.z), 
      length(player->speed));
    // vec2 target_pos = vec2(player->position.x, player->position.z);
    target = vec3(target_pos.x, player->position.y, target_pos.y);
  }

  RotateSpider(creature, target, 0.99f);

  vec3 dir = normalize(target - creature->position);
  dir.y = -0.1f;

  if (int(creature->frame) >= 43 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 p2 = creature->position + dir * 200.0f;

    shared_ptr<Mesh> mesh = resources_->GetMesh(creature);
    int bone_id = mesh->bones_to_ids["tail4"];
    BoundingSphere s = creature->GetBoneBoundingSphere(bone_id);

    resources_->CastMissile(creature, s.center, MISSILE_BOUNCYBALL, 
      dir, missile_speed);

    if (!action->no_cooldown) {
      creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;
    }

    resources_->CreateParticleEffect(10, s.center,
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 3.0, 24.0f, 15.0f, "fireball");          
  }
  return false;
}

bool AI::RedMetalEyeAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const int num_missiles = 1;
  const float missile_speed = 1.0f;

  ObjPtr target_unit = creature->GetCurrentTarget();

  if (!action->initiated) {
    action->initiated = true;
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
  }

  bool is_rotating = RotateSpider(creature, target_unit->position, 0.8f);
  if (is_rotating) {
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
    return false;
  }

  resources_->ChangeObjectAnimation(creature, "Armature|attack", true, TRANSITION_NONE);

  int num_frames = creature->GetNumFramesInCurrentAnimation();
  if (int(creature->frame) >= num_frames - 3) {
    return true;
  }

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position;

  // Predict player pos.
  if (length(player->speed) > 0.01) {
    vec2 target_pos = PredictMissileHitLocation(vec2(creature->position.x, creature->position.z), 
      missile_speed, vec2(player->position.x, player->position.z), 
      vec2(player->speed.x, player->speed.z), 
      length(player->speed));
    target = vec3(target_pos.x, player->position.y, target_pos.y);
  }

  RotateSpider(creature, target, 0.99f);

  vec3 dir = normalize(target - creature->position);
  dir.y = 0.1f;

  if (int(creature->frame) >= 41 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 p2 = creature->position + dir * 200.0f;
    resources_->CastMissile(creature, 
      creature->position + vec3(0, 2, 0), MISSILE_RED_METAL_EYE, dir, 
      missile_speed);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;

    shared_ptr<Mesh> mesh = resources_->GetMesh(creature);
    int bone_id = mesh->bones_to_ids["muzzle_bone"];
    BoundingSphere s = creature->GetBoneBoundingSphere(bone_id);
    resources_->CreateParticleEffect(10, s.center,
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 3.0, 24.0f, 15.0f, "fireball");          
  }
  return false;
}

bool AI::MetalEyeAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const int num_missiles = 1;
  const float missile_speed = 1.8f + Random(0, 5) * 0.3f;

  ObjPtr target_unit = creature->GetCurrentTarget();

  if (!action->initiated) {
    action->initiated = true;
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
  }

  bool is_rotating = RotateSpider(creature, target_unit->position, 0.8f);
  if (is_rotating) {
    resources_->ChangeObjectAnimation(creature, "Armature|idle", true,
      TRANSITION_FINISH_ANIMATION);
    return false;
  }

  resources_->ChangeObjectAnimation(creature, "Armature|attack", true, TRANSITION_NONE);

  int num_frames = creature->GetNumFramesInCurrentAnimation();
  if (int(creature->frame) >= num_frames - 3) {
    return true;
  }

  ObjPtr player = resources_->GetPlayer();
  vec3 target = player->position;

  // Predict player pos.
  if (length(player->speed) > 0.01) {
    vec2 target_pos = PredictMissileHitLocation(vec2(creature->position.x, creature->position.z), 
      missile_speed, vec2(player->position.x, player->position.z), 
      vec2(player->speed.x, player->speed.z), 
      length(player->speed));
    target = vec3(target_pos.x, player->position.y, target_pos.y);
  }

  RotateSpider(creature, target, 0.99f);

  vec3 dir = normalize(target - creature->position);
  dir.y -= 0.1f;

  if (int(creature->frame) >= 41 && !action->damage_dealt) {
    action->damage_dealt = true;
    vec3 p2 = creature->position + dir * 200.0f;
    resources_->CastMissile(creature, creature->position + vec3(0, 2, 0), MISSILE_BOUNCYBALL, 
      dir, missile_speed);
    creature->cooldowns["ranged-attack"] = glfwGetTime() + 1.5;

    shared_ptr<Mesh> mesh = resources_->GetMesh(creature);
    int bone_id = mesh->bones_to_ids["muzzle_bone"];
    BoundingSphere s = creature->GetBoneBoundingSphere(bone_id);
    resources_->CreateParticleEffect(10, s.center,
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 3.0, 24.0f, 15.0f, "fireball");          
  }
  return false;
}

bool AI::WraithAttack(ObjPtr creature, 
  shared_ptr<RangedAttackAction> action) {
  const int num_missiles = boost::lexical_cast<int>(resources_->GetGameFlag("white_spine_num_missiles"));
  const float missile_speed = boost::lexical_cast<float>(resources_->GetGameFlag("white_spine_missile_speed"));
  const float spread = boost::lexical_cast<float>(resources_->GetGameFlag("white_spine_missile_spread"));

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 58) return true;

  if (creature->frame >= 25 && !action->damage_dealt) {
    action->damage_dealt = true;
    ObjPtr player = resources_->GetObjectByName("player");

    BoundingSphere s = creature->GetBoneBoundingSphereByBoneName("R.hand");
    vec3 dir = CalculateMissileDirectionToHitTarget(s.center,
      player->position + vec3(0, -3.0f, 0), missile_speed);

    for (int i = 0; i < num_missiles; i++) {
      vec3 p2 = creature->position + dir * 200.0f;
      vec3 dir = normalize(p2 - creature->position);

      if (i > 0) {
        float x_ang = (float) Random(-5, 6) * spread;
        float y_ang = (float) Random(-5, 6) * spread;
        vec3 right = cross(dir, vec3(0, 1, 0));
        mat4 m = rotate(mat4(1.0f), x_ang, vec3(0, 1, 0));
        dir = vec3(rotate(m, y_ang, right) * vec4(dir, 1.0f));
      }

      resources_->CastMissile(creature, s.center, MISSILE_HORN, dir, missile_speed);
    }

    creature->cooldowns["ranged-attack"] = glfwGetTime() + 2.0;
  }
  return false;
}

bool AI::ProcessMeeleeAttackAction(ObjPtr spider, 
  shared_ptr<MeeleeAttackAction> action) {
  if (resources_->GetConfigs()->disable_attacks) {
    return true;
  }

  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  int num_frames = spider->GetNumFramesInCurrentAnimation();
  if (spider->frame >= num_frames - 5) {
    return true;
  }

  // if (int(spider->frame) == 44) {
  if (!action->damage_dealt) {
    ObjPtr target = spider->GetCurrentTarget();

    cout << "Attacking player" << endl;
    vec3 dir = target->position - spider->position;
    if (length(dir) < 20.0f) {
      cout << "Meelee attack hit: " << length(dir) << endl;
      spider->MeeleeAttack(target, normalize(dir));
      action->damage_dealt = true;
      target->speed += normalize(target->position - spider->position) * 0.3f;
    }
  }
  return false;
}

bool AI::ProcessSweepAttackAction(ObjPtr spider, 
  shared_ptr<SweepAttackAction> action) {
  if (resources_->GetConfigs()->disable_attacks) {
    return true;
  }

  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  int num_frames = spider->GetNumFramesInCurrentAnimation();
  if (spider->frame >= num_frames - 5) {
    return true;
  }

  if (!action->damage_dealt && spider->frame > 20) {
    ObjPtr target = spider->GetCurrentTarget();

    vec3 dir = target->position - spider->position;
    if (length(dir) < 40.0f) {
      spider->MeeleeAttack(target, normalize(dir));
      action->damage_dealt = true;
      target->speed += normalize(target->position - spider->position) * 0.5f;
    }
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
    if (x == 0 && y == 0) continue;

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
  ObjPtr target = spider->GetCurrentTarget();

  if (spider->levitating) {
    resources_->ChangeObjectAnimation(spider, "Armature|climbing");
  } else {
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
  }

  bool is_rotating = RotateSpider(spider, target->position, 0.9f);
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

bool AI::ProcessMoveAction(ObjPtr spider, vec3 destination) {
  vec3 to_next_location = destination - spider->position;

  // TODO: create function GetPhysicsBehavior.
  PhysicsBehavior physics_behavior = spider->physics_behavior;
  if (physics_behavior == PHYSICS_UNDEFINED) {
    if (spider->GetAsset()) {
      physics_behavior = spider->GetAsset()->physics_behavior;
    }
  }

  shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
    int num_frames = GetNumFramesInAnimation(*mesh, 
      spider->active_animation);

  float dist_next_location = length(vec3(to_next_location.x, 0, 
    to_next_location.z));
  if (dist_next_location < 1.0f) {
    return true;
  }

  if (spider->GetAsset()->name == "scorpion" ||
      spider->GetAsset()->name == "shooter_bug" ||
      spider->GetAsset()->name == "metal_eye") {
    int movement_type = 0;
    bool is_rotating = RotateSpiderWithStrafe(spider, destination, 
      &movement_type, 0.75);

    vector<string> animations { "Armature|walking", "Armature|strafe_right", 
      "Armature|strafe_left" };
    resources_->ChangeObjectAnimation(spider, animations[movement_type], 
      TRANSITION_FINISH_ANIMATION);

    if (is_rotating) {
      return false;
    }
  } else {
    bool is_rotating = RotateSpider(spider, destination, 0.75);
    if (is_rotating) {
      return false;
    }

    resources_->ChangeObjectAnimation(spider, "Armature|walking");
  }

  float speed = spider->current_speed;
  if (physics_behavior == PHYSICS_FLY) {
    vec3 v = normalize(to_next_location);
    spider->speed += v * speed;

    // if (spider->position.y > kDungeonOffset.y + 20.0f) {
    //   spider->speed.y += -1.0f * speed * 0.25f;
    // } else if (spider->position.y < kDungeonOffset.y + 5.0f) {
    //   spider->speed.y += 1.0f * speed * 0.25f;
    // } else {
    //   if (Random(0, 2) == 0) {
    //     spider->speed.y += -speed * 0.1f;
    //   } else {
    //     spider->speed.y += speed * 0.25f;
    //   }
    // }

    // if (dist_next_location < 10.0f && spider->GetAsset()->name == "blood_worm") {
    if (dist_next_location < 10.0f) {
      spider->speed.x *= 0.9;
      spider->speed.z *= 0.9;
    }
  } else {
    if (!spider->touching_the_ground) {
      return true;
    }

    spider->speed += spider->forward * speed;
  }
  return false;
}

bool AI::ProcessMoveAction(ObjPtr spider, shared_ptr<MoveAction> action) {
  if (spider->GetPhysicsBehavior() == PHYSICS_FLY) {
    if (glfwGetTime() > action->issued_at + 0.5f) {
      return true;
    }
  } else if (glfwGetTime() > action->issued_at + 2.0f) {
    spider->actions.push(make_shared<RandomMoveAction>());
    return true;
  }

  return ProcessMoveAction(spider, action->destination);
}

bool AI::ProcessIdleAction(ObjPtr spider, shared_ptr<IdleAction> action) {
  resources_->ChangeObjectAnimation(spider, action->animation);
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

  ObjPtr target = spider->GetCurrentTarget();
  const vec3& player_pos = target->position;

  float min_distance = 0.0f;

  float flight_height = 3.0f;
  // if (spider->GetPhysicsBehavior() == PHYSICS_FLY) {
  //   if (spider->position.y < 20.0f) flight_height = spider->position.y + 1.0f;
  //   else flight_height = spider->position.y + Random(-2, 2) * 1.0f;
  // }

  vec3 next_pos;
  float t;
  if (dungeon.IsMovementObstructed(spider->position, player_pos, t)) {
    next_pos = dungeon.GetNextMove(spider->position, player_pos, 
      min_distance);

    char tile = dungeon.GetTileAt(next_pos).ascii_code;
    switch (tile) {
      case 'd': case 'D': case 'o': case 'O': {
        flight_height = 3.0f;
        break;
      }
    }

    // Use direct pathfinding when the monster is very close.
    if (length2(next_pos) < 0.0001f) {
      next_pos = spider->position + normalize(player_pos - spider->position) * 1.0f;
    }
  } else {
    vec3 v = player_pos - spider->position;
    if (length(v) < 3.0f) {
      next_pos = player_pos;
    } else {
      next_pos = spider->position + normalize(v) * 3.0f;
    }
  }

  next_pos.y = flight_height;
  spider->actions.push(make_shared<MoveAction>(next_pos));
  return true;
}

bool AI::ProcessLongMoveAction(
  ObjPtr spider, shared_ptr<LongMoveAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();
  const vec3& dest = action->destination;

  float min_distance = 0.0f;

  // float current_time = glfwGetTime();
  // if (current_time > action->last_update + 1) { 
  //   // The spider has practically not moved in 1 second. No point in proceeding 
  //   // with this action.
  //   if (length(spider->position - action->last_position) < 0.2f) {
  //     return true;
  //   }

  //   action->last_update = current_time;
  //   action->last_position = spider->position;
  // }

  float flight_height = 3.0f;
  // if (spider->GetPhysicsBehavior() == PHYSICS_FLY) {
  //   if (spider->position.y < 20.0f) flight_height = spider->position.y + 1.0f;
  //   else flight_height = spider->position.y + Random(-2, 2) * 1.0f;
  // }

  vec3 next_pos;
  float t;
  if (dungeon.IsMovementObstructed(spider->position, dest, t)) {
    next_pos = dungeon.GetNextMove(spider->position, dest, 
      min_distance);

    char tile = dungeon.GetTileAt(next_pos).ascii_code;
    switch (tile) {
      case 'd': case 'D': case 'o': case 'O': {
        flight_height = 3.0f;
        break;
      }
    }

    ivec2 dungeon_tile = dungeon.GetDungeonTile(spider->position);
    ivec2 source_tile = dungeon.GetClosestClearTile(spider->position);

    if (length2(next_pos) < 0.0001f) {
      next_pos = spider->position + normalize(dest - spider->position) * 1.0f;
    }
  } else {
    vec3 v = dest - spider->position;
    if (length(v) < action->min_distance) {
      next_pos = dest;
    } else {
      next_pos = spider->position + normalize(v) * 3.0f;
    }
  }

  vec3 v = dest - spider->position;
  if (length(v) < action->min_distance) {
    return true;
  }

  next_pos.y = flight_height;
  ProcessMoveAction(spider, next_pos);

  return false;
}

bool AI::ProcessSpiderClimbAction(ObjPtr spider, 
  shared_ptr<SpiderClimbAction> action) {
  spider->can_jump = false;
  if (spider->frame >= 25 && !action->finished_jump) {
    action->finished_jump = true;
  } else if (!action->finished_jump) {
    resources_->ChangeObjectAnimation(spider, "Armature|climb");
  } else {
    spider->AddTemporaryStatus(make_shared<SpiderThreadStatus>(5.0f, 20.0f));
    resources_->ChangeObjectAnimation(spider, "Armature|climbing");
  }

  // TODO: change animation to climb.

  float distance_to_ceiling = kDungeonOffset.y + action->height - spider->position.y;
  spider->levitating = true;

  // TODO: could be climbing speed.
  if (distance_to_ceiling < 1.0f) {
    spider->levitating = false;
    spider->ClearTemporaryStatus(STATUS_SPIDER_THREAD);
    return true;
  }

  spider->speed.y += 0.5f * spider->current_speed;
  return false;
}

bool AI::ProcessSpiderJumpAction(ObjPtr spider, 
  shared_ptr<SpiderJumpAction> action) {
  if (!action->finished_rotating) {
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    bool is_rotating = RotateSpider(spider, action->destination, 0.99f);
    if (is_rotating) {
      return false;
    }
    action->finished_rotating = true;
  }

  if (!spider->can_jump) {
    return true;
  }

  if (!action->finished_jump) {
    Dungeon& dungeon = resources_->GetDungeon();
    float t;
    if (dungeon.IsMovementObstructed(spider->position, action->destination, t)) {
      return true;
    }

    spider->frame = 0;
    resources_->ChangeObjectAnimation(spider, "Armature|jump");
    vec3 v = action->destination - spider->position;
    v.y = 0;
    if (spider->GetAsset()->name == "goblin_chieftain") {
      spider->speed += normalize(v) * 0.9f + vec3(0, 0.6f, 0);
    } else {
      spider->speed += normalize(v) * 0.5f + vec3(0, 0.5f, 0);
    }
    action->finished_jump = true;
    spider->can_jump = false;
  } else if (spider->frame >= 35) {
    return true;
  } 
  return false;
}

bool AI::ProcessFrogShortJumpAction(ObjPtr spider, 
  shared_ptr<FrogShortJumpAction> action) {
  Dungeon& dungeon = resources_->GetDungeon();

  if (!action->finished_rotating) {
    if (!spider->can_jump) {
      return true;
    }

    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    bool is_rotating = RotateSpider(spider, action->destination, 0.99f);
    if (is_rotating) {
      return false;
    }
    action->finished_rotating = true;
  }

  if (!action->finished_jump) {
    if (!spider->can_jump) {
      return true;
    }

    float t;
    if (dungeon.IsMovementObstructed(spider->position, action->destination, t)) {
      return true;
    }

    spider->frame = 0;
    resources_->ChangeObjectAnimation(spider, "Armature|jump");
    vec3 v = action->destination - spider->position;
    v.y = 0;
    spider->speed += normalize(v) * 0.5f + vec3(0, 0.7f, 0);
    spider->can_jump = false;

    action->finished_jump = true;
    return false;
  } 

  if (spider->can_jump) {
    return true;
  } 
  return false;
}

bool AI::ProcessRedMetalSpinAction(ObjPtr spider, 
  shared_ptr<RedMetalSpinAction> action) {
  const int num_missiles = 1;
  const float missile_speed = 0.8f;

  resources_->ChangeObjectAnimation(spider, "Armature|spin");

  if (!action->started) {
    action->started = true;
    action->shot_countdown = glfwGetTime() + 1;
    action->shot_2_countdown = glfwGetTime() + 1.5;
  }

  if (glfwGetTime() > action->shot_countdown + 2) {
    spider->cooldowns["spin"] = glfwGetTime() + 8;
    return true;
  }

  if (glfwGetTime() > action->shot_countdown && !action->shot) {
    action->shot = true;

    float rotation = 0.0f;
    vec3 pos = spider->position;
    pos.y = kDungeonOffset.y;
    for (int i = 0; i < 18; i++) {
      mat4 rotation_matrix = rotate(
        mat4(1.0),
        rotation,
        vec3(0.0f, 1.0f, 0.0f)
      );

      vec3 dir = vec3(rotation_matrix * vec4(0, 0.5f, 1, 1));
      shared_ptr<Missile> missile = resources_->CastMissile(spider, 
        spider->position + dir * 3.0f + vec3(0, 2, 0), MISSILE_RED_METAL_EYE, 
        dir, missile_speed);
      missile->life = 120.0f;
      rotation += 6.28f / 18.0f;
    }
  }

  if (glfwGetTime() > action->shot_2_countdown && !action->shot_2) {
    action->shot_2 = true;

    float rotation = 0.5f * 6.28f / 18.0f;
    vec3 pos = spider->position;
    pos.y = kDungeonOffset.y;
    for (int i = 0; i < 18; i++) {
      mat4 rotation_matrix = rotate(
        mat4(1.0),
        rotation,
        vec3(0.0f, 1.0f, 0.0f)
      );

      vec3 dir = vec3(rotation_matrix * vec4(0, 0.25f, 1, 1));
      shared_ptr<Missile> missile = resources_->CastMissile(spider, 
        spider->position + dir * 3.0f + vec3(0, 2, 0), MISSILE_RED_METAL_EYE, 
        dir, missile_speed);
      missile->life = 120.0f;
      rotation += 6.28f / 18.0f;
    }
  }

  return false;
}

bool AI::ProcessFrogJumpAction(ObjPtr spider, 
  shared_ptr<FrogJumpAction> action) {
  if (!action->finished_rotating) {
    if (!spider->can_jump) {
      return true;
    }

    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    bool is_rotating = RotateSpider(spider, action->destination, 0.99f);
    if (is_rotating) {
      return false;
    }
    action->finished_rotating = true;

    if (spider->GetAsset()->name == "red_frog") {
      action->chanel_until = glfwGetTime() + 0.5;
    } else {
      action->chanel_until = glfwGetTime() + 1.0;
    }
  }

  if (glfwGetTime() < action->chanel_until) {
    resources_->CreateParticleEffect(1, spider->position + vec3(0, 3, 0), 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 1.0, 24.0f, 15.0f, "fireball");          
    return false;
  }

  if (!action->finished_jump) {
    if (!spider->can_jump) {
      return true;
    }

    Dungeon& dungeon = resources_->GetDungeon();
    float t;

    spider->frame = 0;
    resources_->ChangeObjectAnimation(spider, "Armature|jump");
    vec3 v = resources_->GetPlayer()->position - spider->position;
    v.y = 0;
    spider->speed += normalize(v) * 1.5f + vec3(0, 0.3f, 0);
    action->finished_jump = true;
    spider->can_jump = false;
  } else if (spider->frame >= 35) {
    return true;
  } 
  return false;
}

bool AI::ProcessChargeAction(ObjPtr spider, 
  shared_ptr<ChargeAction> action) {
  if (!action->finished_rotating) {
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    bool is_rotating = RotateSpider(spider, resources_->GetPlayer()->position, 0.99f);
    if (is_rotating) {
      return false;
    }
    action->finished_rotating = true;

    action->channel_until = glfwGetTime() + 1;
  }

  if (glfwGetTime() < action->channel_until) {
    resources_->CreateParticleEffect(1, spider->position + vec3(0, 3, 0), 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 1.0, 24.0f, 15.0f, "fireball");          
    return false;
  }

  if (!action->damage_dealt) {
    spider->cooldowns["charge"] = glfwGetTime() + 10;
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    spider->frame = 0;
    vec3 v = resources_->GetPlayer()->position - spider->position;
    v.y = 0;
    spider->speed += normalize(v) * 5.0f;
    action->damage_dealt = true;
    return false;
  } 

  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  int num_frames = spider->GetNumFramesInCurrentAnimation();
  if (spider->frame >= num_frames - 5) {
    return true;
  }

  if (!action->damage_dealt && spider->frame > 20) {
    ObjPtr target = spider->GetCurrentTarget();

    vec3 dir = target->position - spider->position;
    if (length(dir) < 40.0f) {
      spider->MeeleeAttack(target, normalize(dir));
      action->damage_dealt = true;
      target->speed += normalize(target->position - spider->position) * 0.5f;
    }
  }
  return false;
}

bool AI::ProcessTrampleAction(ObjPtr spider, 
  shared_ptr<TrampleAction> action) {
  shared_ptr<Player> player = resources_->GetPlayer();
  Dungeon& dungeon = resources_->GetDungeon();

  if (!action->finished_rotating) {
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    bool is_rotating = RotateSpider(spider, resources_->GetPlayer()->position, 0.99f);
    if (is_rotating) {
      return false;
    }
    action->finished_rotating = true;

    action->channel_until = glfwGetTime() + 1;
  }

  if (glfwGetTime() < action->channel_until) {
    resources_->CreateParticleEffect(1, spider->position + vec3(0, 3, 0), 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 1.0, 24.0f, 15.0f, "fireball");          
    return false;
  }

  if (!action->damage_dealt) {
    spider->cooldowns["trample"] = glfwGetTime() + 5;
    resources_->ChangeObjectAnimation(spider, "Armature|walking");
    spider->frame = 0;

    vec2 target_pos = PredictMissileHitLocation(
      vec2(spider->position.x, spider->position.z), 
      2.7f, vec2(player->position.x, player->position.z), 
      vec2(player->speed.x, player->speed.z), 
      length(player->speed));

    action->target = vec3(target_pos.x, player->position.y, target_pos.y);
    action->damage_dealt = true;
    return false;
  } 

  if (action->damage_dealt) {
    vec3 v = action->target - spider->position;
    v.y = 0;
 
    ivec2 spider_tile = dungeon.GetDungeonTile(spider->position);
    if (spider_tile.x < 2 || spider_tile.y < 2 ||
        spider_tile.x > 12 || spider_tile.y > 12) {
      spider->speed *= 0.3f;
      return true;
    }

    if (length(v) < 5.0f) {
      spider->speed *= 0.3f;
      return true;
    }

    spider->speed = normalize(v) * 3.0f;
    return false;
  }

  return false;
}

bool AI::ProcessSpiderEggAction(ObjPtr spider, 
  shared_ptr<SpiderEggAction> action) {
  resources_->ChangeObjectAnimation(spider, "Armature|walking");
 
  if (!action->created_particle_effect) {
    auto bone = spider->bones[23];
    shared_ptr<Particle> p = resources_->CreateOneParticle(bone.bs.center, 300.0f, 
      "particle-sparkle-fire", 2.0f);
    p->associated_obj = spider;
    p->offset = vec3(0);
    p->associated_bone = 23;

    action->channel_end = glfwGetTime() + 4.0f;
    action->created_particle_effect = true;
  }

  if (glfwGetTime() > action->channel_end) {
    resources_->CastSpiderEgg(spider);
    spider->cooldowns["spider-egg"] = glfwGetTime() + 3;
    return true;
  }

  if (ProcessMoveAction(spider, action->target)) {
    Dungeon& dungeon = resources_->GetDungeon();
    ivec2 origin = dungeon.GetDungeonTile(spider->position);
    ivec2 player_tile = dungeon.GetDungeonTile(resources_->GetPlayer()->position);

    int padding = 4;
    bool move_to_center = origin.x < padding || origin.y < padding ||
                          origin.x >= 14 - padding || origin.y >= 14 - padding;
    if (move_to_center) { 
      origin = ivec2(7, 7);
    }    

    ivec2 tile = ivec2(7, 7);
    for (int tries = 0; tries < 10; tries++) {
      int x = Random(1, 3);
      int y = Random(1, 3);
      ivec2 new_tile = origin + ivec2(x, y);
      if (!dungeon.IsValidTile(new_tile)) continue;
      if (length(vec2(player_tile) - vec2(new_tile)) < 5.0f) continue;
      tile = new_tile;
    }
    action->target = dungeon.GetTilePosition(tile);
  }

  return false;
}

bool AI::ProcessWormBreedAction(ObjPtr spider, 
  shared_ptr<WormBreedAction> action) {
  resources_->ChangeObjectAnimation(spider, "Armature|walking");

  if (!action->created_particle_effect) {
    auto bone = spider->bones[0];
    shared_ptr<Particle> p = resources_->CreateOneParticle(bone.bs.center, 300.0f, 
      "particle-sparkle-fire", 2.0f);
    p->associated_obj = spider;
    p->offset = vec3(0);
    p->associated_bone = 23;

    action->channel_end = glfwGetTime() + 5.0f;
    action->created_particle_effect = true;
  }

  if (glfwGetTime() > action->channel_end) {
    resources_->CastWormBreed(spider);
    return true;
  }

  return false;
}

bool AI::ProcessSpiderWebAction(ObjPtr creature, 
  shared_ptr<SpiderWebAction> action) {
  if (resources_->GetConfigs()->disable_attacks) {
    return true;
  }

  if (!creature->CanUseAbility("spider-web")) {
    return true;
  }

  resources_->ChangeObjectAnimation(creature, "Armature|attack");

  if (int(creature->frame) >= 40) return true;

  if (creature->frame >= 30 && !action->cast_complete) {
    action->cast_complete = true;
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = CalculateMissileDirectionToHitTarget(creature->position,
      action->target, 2.0);
    resources_->CastSpiderWebShot(creature, dir);
    creature->cooldowns["spider-web"] = glfwGetTime() + 5;
  }
  return false;
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
  shared_ptr<Configs> configs = resources_->GetConfigs();

  cout << spider->GetName() << " is using " << action->ability << endl;
  resources_->ChangeObjectAnimation(spider, "Armature|attack");

  Dungeon& dungeon = resources_->GetDungeon();
  ivec2 spider_tile = dungeon.GetDungeonTile(spider->position);

  ObjPtr player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  float current_time = glfwGetTime();
  if (spider->cooldowns.find(action->ability) != spider->cooldowns.end()) {
    if (spider->cooldowns[action->ability] > current_time) {
      return true;
    }
  }

  // TODO: maybe call script with ability effect.
  if (action->ability == "summon-spiderling") {
    if (configs->summoned_creatures > 10) return true;
    int num_summons = 2;
    for (int i = 0; i < 100; i++) {
      int off_x = Random(-2, 2);
      int off_y = Random(-2, 2);
      ivec2 tile = spider_tile + ivec2(off_x, off_y);
      if (!dungeon.IsTileClear(tile)) continue;
     
      vec3 tile_pos = dungeon.GetTilePosition(tile);
      ObjPtr spiderling = CreateGameObjFromAsset(resources_.get(), "spiderling", tile_pos);
      spiderling->ai_state = AI_ATTACK;
      spiderling->summoned = true;
      resources_->CreateParticleEffect(32, tile_pos, 
        vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
      configs->summoned_creatures++;
      if (--num_summons == 0) break;
    }
    spider->cooldowns["summon-spiderling"] = glfwGetTime() + 15;
  } else if (action->ability == "haste") {
    spider->AddTemporaryStatus(make_shared<HasteStatus>(2.0, 5.0f, 1));
    spider->cooldowns["haste"] = glfwGetTime() + 10;
  } else if (action->ability == "invisibility") {
    spider->AddTemporaryStatus(make_shared<InvisibilityStatus>(10.0f, 1));
    resources_->CreateParticleEffect(1, spider->position, 
      vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 7.0, 32.0f, 3.0f);
    spider->cooldowns["invisibility"] = glfwGetTime() + 60;
  } else if (action->ability == "hook") {
    resources_->CastHook(spider, spider->position, 
      normalize(player->position - spider->position));
    spider->cooldowns["hook"] = glfwGetTime() + 10;
  } else if (action->ability == "acid-arrow") {
    resources_->CastAcidArrow(spider, spider->position, 
      normalize(player->position - spider->position));
    spider->cooldowns["acid-arrow"] = glfwGetTime() + 1;
  } else if (action->ability == "lightning-ray") {
    resources_->CastMagmaRay(spider, spider->position, 
      normalize(player->position - spider->position - vec3(0, 3, 0)));
    spider->cooldowns["lightning-ray"] = glfwGetTime() - 1;
  } else if (action->ability == "blinding-ray") {
    resources_->CastBlindingRay(spider, spider->position, 
      normalize(player->position - spider->position - vec3(0, 3, 0)));
    spider->cooldowns["blinding-ray"] = glfwGetTime() + 20;
  } else if (action->ability == "teleport-back") {
    for (int i = 0; i < 100; i++) {
      int off_x = Random(-2, 2);
      int off_y = Random(-2, 2);
      ivec2 tile = spider_tile + ivec2(off_x, off_y);
      if (!dungeon.IsTileClear(tile)) continue;
     
      vec3 tile_pos = dungeon.GetTilePosition(tile);
      player->position = tile_pos;
      break;
    }
    spider->cooldowns["teleport-back"] = glfwGetTime() + 20;
  } else if (action->ability == "confusion-mushroom") {
    int cur_room = dungeon.GetRoom(spider_tile);
    int player_room = dungeon.GetRoom(player_tile);

    if (cur_room == player_room && length(vec2(spider_tile - player_tile)) < 5) {
      // TODO: add confusion status.
      player->AddTemporaryStatus(make_shared<PoisonStatus>(1.0f, 20.0f, 1));
    }
    spider->cooldowns["confusion-mushroom"] = glfwGetTime() + 30;
  } else if (action->ability == "mold-poison") {
    int cur_room = dungeon.GetRoom(spider_tile);
    for (int x = -5; x <= 5; x++) {
      for (int y = -5; y <= 5; y++) {
        if (length(vec2(x, y)) > 5) continue;
        ivec2 cur_tile = spider_tile + ivec2(x, y); 
        if (!dungeon.IsValidTile(cur_tile)) continue;
        if (dungeon.GetRoom(cur_tile) != cur_room) continue;

        vec3 tile_pos = dungeon.GetTilePosition(cur_tile);
        resources_->CreateParticleEffect(4, tile_pos, 
          vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);

        if (cur_tile.x == player_tile.x && cur_tile.y == player_tile.y) {
          player->AddTemporaryStatus(make_shared<PoisonStatus>(1.0f, 20.0f, 1));
        }
      }
    }

    // spider->AddTemporaryStatus(make_shared<PoisonStatus>(2.0, 5.0f, 1));
    spider->cooldowns["mold-poison"] = glfwGetTime() + 10;
  } else if (action->ability == "summon-worm") {
    if (configs->summoned_creatures > 10) return true;
    for (int tries = 0; tries < 100; tries++) {
      int off_x = Random(-1, 1);
      int off_y = Random(-1, 1);
      ivec2 tile = spider_tile + ivec2(off_x, off_y);
      if (!dungeon.IsTileClear(tile)) continue;
     
      vec3 tile_pos = dungeon.GetTilePosition(tile);
      ObjPtr spiderling = CreateGameObjFromAsset(resources_.get(), "worm", tile_pos);
      spiderling->ai_state = AI_ATTACK;
      spiderling->summoned = true;
      resources_->CreateParticleEffect(32, tile_pos, 
        vec3(0, 2.0f, 0), vec3(0.6, 0.2, 0.8), 5.0, 60.0f, 10.0f);
      configs->summoned_creatures++;
      break;
    }
    
    if (spider->GetAsset()->name == "worm_king") {
      spider->cooldowns["summon-worm"] = glfwGetTime() + 5;
    } else {
      spider->cooldowns["summon-worm"] = glfwGetTime() + 30;
    }
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
  } else if (action->ability == "jump") {
    spider->AddTemporaryStatus(make_shared<HasteStatus>(2.0, 5.0f, 1));
    spider->cooldowns["haste"] = glfwGetTime() + 10;
  } else if (action->ability == "string-attack") {
    ObjPtr player = resources_->GetObjectByName("player");
    vec3 dir = normalize((player->position + vec3(0, -3.0f, 0)) - spider->position);
    resources_->CastStringAttack(spider, spider->position + vec3(0, 2, 0), dir);
    // spider->cooldowns["string-attack"] = glfwGetTime() + 1;

    // TODO: set duration for cast ability.
    if (glfwGetTime() > action->issued_at + 2) {
      return true;
    } else {
      return false;
    }
  }
  return true;
}

// An action should be precise and direct to the point.
void AI::ProcessNextAction(ObjPtr spider) {
  if (spider->actions.empty()) {
    return;
  }

  if (spider->transition_animation) return;

  shared_ptr<Action> action = spider->actions.front();
  switch (action->type) {
    case ACTION_MOVE: {
      shared_ptr<MoveAction> move_action =  
        static_pointer_cast<MoveAction>(action);
      if (ProcessMoveAction(spider, move_action)) {
        spider->PopAction();
        // shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
        //   int num_frames = GetNumFramesInAnimation(*mesh, 
        //     spider->active_animation);
        // if (spider->frame >= num_frames) {
        //   spider->frame = 0;
        // }
      }
      break;
    }
    case ACTION_LONG_MOVE: {
      shared_ptr<LongMoveAction> move_action =  
        static_pointer_cast<LongMoveAction>(action);
      if (ProcessLongMoveAction(spider, move_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_RANDOM_MOVE: {
      shared_ptr<RandomMoveAction> random_move_action =  
        static_pointer_cast<RandomMoveAction>(action);
      if (ProcessRandomMoveAction(spider, random_move_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_IDLE: {
      shared_ptr<IdleAction> idle_action =  
        static_pointer_cast<IdleAction>(action);
      if (ProcessIdleAction(spider, idle_action)) {
        spider->PopAction();
      }
      break;
    }
    case ACTION_TAKE_AIM: {
      shared_ptr<TakeAimAction> take_aim_action =  
        static_pointer_cast<TakeAimAction>(action);
      if (ProcessTakeAimAction(spider, take_aim_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_MEELEE_ATTACK: {
      shared_ptr<MeeleeAttackAction> meelee_attack_action =  
        static_pointer_cast<MeeleeAttackAction>(action);
      if (ProcessMeeleeAttackAction(spider, meelee_attack_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_CHARGE: {
      shared_ptr<ChargeAction> charge_action =  
        static_pointer_cast<ChargeAction>(action);
      if (ProcessChargeAction(spider, charge_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_TRAMPLE: {
      shared_ptr<TrampleAction> trample_action =  
        static_pointer_cast<TrampleAction>(action);
      if (ProcessTrampleAction(spider, trample_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_SWEEP_ATTACK: {
      shared_ptr<SweepAttackAction> sweep_attack_action =  
        static_pointer_cast<SweepAttackAction>(action);
      if (ProcessSweepAttackAction(spider, sweep_attack_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_SPIDER_CLIMB: {
      shared_ptr<SpiderClimbAction> spider_climb_action =  
        static_pointer_cast<SpiderClimbAction>(action);
      if (ProcessSpiderClimbAction(spider, spider_climb_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_SPIDER_JUMP: {
      shared_ptr<SpiderJumpAction> spider_jump_action =  
        static_pointer_cast<SpiderJumpAction>(action);
      if (ProcessSpiderJumpAction(spider, spider_jump_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_FROG_JUMP: {
      shared_ptr<FrogJumpAction> frog_jump_action =  
        static_pointer_cast<FrogJumpAction>(action);
      if (ProcessFrogJumpAction(spider, frog_jump_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_FROG_SHORT_JUMP: {
      shared_ptr<FrogShortJumpAction> frog_short_jump_action =  
        static_pointer_cast<FrogShortJumpAction>(action);
      if (ProcessFrogShortJumpAction(spider, frog_short_jump_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_RED_METAL_SPIN: {
      shared_ptr<RedMetalSpinAction> red_metal_spin_action =  
        static_pointer_cast<RedMetalSpinAction>(action);
      if (ProcessRedMetalSpinAction(spider, red_metal_spin_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_SPIDER_EGG: {
      shared_ptr<SpiderEggAction> spider_egg_action =  
        static_pointer_cast<SpiderEggAction>(action);
      if (ProcessSpiderEggAction(spider, spider_egg_action)) {
        spider->PopAction();

        shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
          int num_frames = GetNumFramesInAnimation(*mesh, 
            spider->active_animation);
        if (spider->frame >= num_frames) {
          // spider->frame = 0;
        }
      }
      break;
    }
    case ACTION_WORM_BREED: {
      shared_ptr<WormBreedAction> worm_breed_action =  
        static_pointer_cast<WormBreedAction>(action);
      if (ProcessWormBreedAction(spider, worm_breed_action)) {
        spider->PopAction();

        shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
          int num_frames = GetNumFramesInAnimation(*mesh, 
            spider->active_animation);
        if (spider->frame >= num_frames) {
          // spider->frame = 0;
        }
      }
      break;
    }
    case ACTION_SPIDER_WEB: {
      shared_ptr<SpiderWebAction> spider_web_action =  
        static_pointer_cast<SpiderWebAction>(action);
      if (ProcessSpiderWebAction(spider, spider_web_action)) {
        spider->PopAction();

        shared_ptr<Mesh> mesh = resources_->GetMesh(spider);
          int num_frames = GetNumFramesInAnimation(*mesh, 
            spider->active_animation);
        if (spider->frame >= num_frames) {
          // spider->frame = 0;
        }
      }
      break;
    }
    case ACTION_MOVE_TO_PLAYER: {
      shared_ptr<MoveToPlayerAction> move_to_player_action =  
        static_pointer_cast<MoveToPlayerAction>(action);
      if (ProcessMoveToPlayerAction(spider, move_to_player_action)) {
        spider->PopAction();
      }
      break;
    }
    case ACTION_MOVE_AWAY_FROM_PLAYER: {
      shared_ptr<MoveAwayFromPlayerAction> move_away_from_player_action =  
        static_pointer_cast<MoveAwayFromPlayerAction>(action);
      if (ProcessMoveAwayFromPlayerAction(spider, move_away_from_player_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_RANGED_ATTACK: {
      shared_ptr<RangedAttackAction> ranged_attack_action =  
        static_pointer_cast<RangedAttackAction>(action);
      if (ProcessRangedAttackAction(spider, ranged_attack_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_CAST_SPELL: {
      shared_ptr<CastSpellAction> cast_spell_action =  
        static_pointer_cast<CastSpellAction>(action);
      if (ProcessCastSpellAction(spider, cast_spell_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_CHANGE_STATE: {
      shared_ptr<ChangeStateAction> change_state_action =  
        static_pointer_cast<ChangeStateAction>(action);
      if (ProcessChangeStateAction(spider, change_state_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_STAND: {
      shared_ptr<StandAction> stand_action =  
        static_pointer_cast<StandAction>(action);
      if (ProcessStandAction(spider, stand_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_TALK: {
      shared_ptr<TalkAction> talk_action =  
        static_pointer_cast<TalkAction>(action);
      if (ProcessTalkAction(spider, talk_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_ANIMATION: {
      shared_ptr<AnimationAction> animation_action =  
        static_pointer_cast<AnimationAction>(action);
      if (ProcessAnimationAction(spider, animation_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_USE_ABILITY: {
      shared_ptr<UseAbilityAction> use_ability_action =  
        static_pointer_cast<UseAbilityAction>(action);
      if (ProcessUseAbilityAction(spider, use_ability_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_DEFEND: {
      shared_ptr<DefendAction> defend_action =  
        static_pointer_cast<DefendAction>(action);
      if (ProcessDefendAction(spider, defend_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_SIDE_STEP: {
      shared_ptr<SideStepAction> side_step_action =  
        static_pointer_cast<SideStepAction>(action);
      if (ProcessSideStepAction(spider, side_step_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_TELEPORT: {
      shared_ptr<TeleportAction> teleport_action =  
        static_pointer_cast<TeleportAction>(action);
      if (ProcessTeleportAction(spider, teleport_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_FIREBALL: {
      shared_ptr<FireballAction> fireball_action =  
        static_pointer_cast<FireballAction>(action);
      if (ProcessFireballAction(spider, fireball_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_PARALYSIS: {
      shared_ptr<ParalysisAction> paralysis_action =  
        static_pointer_cast<ParalysisAction>(action);
      if (ProcessParalysisAction(spider, paralysis_action)) {
        spider->PopAction();
        // spider->frame = 0;
      }
      break;
    }
    case ACTION_FLY_LOOP: {
      shared_ptr<FlyLoopAction> fly_loop_action =  
        static_pointer_cast<FlyLoopAction>(action);
      if (ProcessFlyLoopAction(spider, fly_loop_action)) {
        spider->PopAction();
      }
      break;
    }
    case ACTION_SPIN: {
      shared_ptr<SpinAction> spin_action =  
        static_pointer_cast<SpinAction>(action);
      if (ProcessSpinAction(spider, spin_action)) {
        spider->PopAction();
      }
      break;
    }
    case ACTION_MIRROR_IMAGE: {
      shared_ptr<MirrorImageAction> mirror_image_action =  
        static_pointer_cast<MirrorImageAction>(action);
      if (ProcessMirrorImageAction(spider, mirror_image_action)) {
        spider->PopAction();
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

  cout << "ProcessPlayerAction" << endl;

  float d = resources_->GetDeltaTime() / 0.016666f;
  switch (action->type) {
    case ACTION_LONG_MOVE: {
      cout << "Long move action" << endl;
      shared_ptr<LongMoveAction> move_action =  
        static_pointer_cast<LongMoveAction>(action);
      if (ProcessLongMoveAction(player, move_action)) {
        player->PopAction();
      }
      break;
    }
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
  for (auto [id, obj] : node->creatures) {
    if (obj->type != GAME_OBJ_DEFAULT) continue;
    if (obj->GetAsset()->type != ASSET_CREATURE) continue;
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
  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (configs->disable_ai) return;

  ivec2 tile = resources_->GetDungeon().GetDungeonTile(resources_->GetPlayer()->position);
  if (!resources_->GetDungeon().IsValidTile(tile)) {
    return;
  }

  RunAiInOctreeNode(resources_->GetOctreeRoot());
  ProcessPlayerAction(resources_->GetPlayer());

  resources_->GetDungeon().CalculateVisibility(
    resources_->GetPlayer()->position);

  // Async.
  while (!ai_tasks_.empty() || running_tasks_ > 0) {
    this_thread::sleep_for(chrono::microseconds(200));
  }

  glfwMakeContextCurrent(resources_->GetWindow());
  return;

  // Sync.
  // while (!ai_tasks_.empty()) {
  //   auto& obj = ai_tasks_.front();
  //   ai_tasks_.pop();
  //   running_tasks_++;
  //   ai_mutex_.unlock();

  //   // TODO: maybe should go to physics.
  //   // if (dot(obj->up, vec3(0, 1, 0)) < 0) {
  //   //   obj->up = vec3(0, 1, 0);
  //   // }

  //   // Check status. If taking hit, dying, poisoned, etc.
  //   if (ProcessStatus(obj)) {
  //     ProcessNextAction(obj);
  //   }

  //   string ai_script = obj->GetAsset()->ai_script;
  //   if (!ai_script.empty()) {
  //     resources_->CallStrFn(ai_script, obj->name);
  //   }

  //   ai_mutex_.lock();
  //   running_tasks_--;
  //   ai_mutex_.unlock();
  // }
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

    ivec2 tile = resources_->GetDungeon().GetDungeonTile(obj->position);
    if (!resources_->GetDungeon().IsValidTile(tile)) {
      if (obj->GetAsset()->name != "big_beholder") {
        ai_mutex_.lock();
        running_tasks_--;
        ai_mutex_.unlock();
      }
      continue;
    }

    // Check status. If taking hit, dying, poisoned, etc.
    if (ProcessStatus(obj)) {
      ProcessNextAction(obj);
    }

    // string ai_script = obj->GetAsset()->ai_script;
    // if (!ai_script.empty()) {
    //   resources_->CallStrFn(ai_script, obj->name);
    // }
    monsters_->RunMonsterAi(obj);

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
